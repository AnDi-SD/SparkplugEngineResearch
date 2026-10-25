# CPU-подготовка кадра: данные, порядок и владение

До device draw игра вычисляет состояние сцены и готовит данные renderer.
Эта статья связывает восстановленный код по его входам и результатам;
подробные формулы и ABI остаются в статьях соответствующих подсистем.
[Граница CPU/API](renderer-boundary.md) отделяет эту работу от shader pipeline
и конечных команд Direct3D.
Наличие конкретных реализаций и отсутствующие полные пути отдельно перечисляет
[карта исходников](source-map.md): описание native поведения не всегда означает,
что вся цепочка уже перенесена в C++.

## Карта восстановленного PC-кода

Имена с `ForAnalysis` обозначают переносимый интерфейс реконструкции.
Они не являются найденными оригинальными C++-сигнатурами. Таблица перечисляет
известные блоки подготовки; полного списка всех CPU-методов движка она не задаёт.

| Блок и код | Вход | Результат и изменяемое состояние | Подробный контракт |
| --- | --- | --- | --- |
| [spVisibilityManager](../../../Sparkplug/Code/Sparkplug/spVisibilityManager.cpp), `46D270` | Камера, partition/объекты, bounding volumes, flags | Frame/zone/root и Base/Octree traversal, отбор объектов, stamp и списки | [Visibility](../visibility/visibility-runtime.md); portal/occluder ветви пока явно unsupported |
| [spSkin](../../../Sparkplug/Code/Sparkplug/spSkin.cpp), `46A240` | Cached bone world, inverse bind, mesh | CPU-палитра костей, указатель и bone count renderer | [Skin render](../animation/skin-render.md) |
| [spParticleSystemRuntime](../../../Sparkplug/Code/Sparkplug/spParticleSystemRuntime.cpp), [CPU update math](../../../Sparkplug/Analysis/PC/spParticleCpuUpdate.h), `48D450/48D210` | Clock delta, пул, скорости/ускорения, lifetime, emission и modifier inputs | Clock, producer, retirement/integration физического кольца, ordered virtual modifier calls | [Обновление частиц](../effects/particle-cpu-update.md); алгоритмы производных modifiers задаются явно |
| [Ballistic CPU draw](../../../Sparkplug/Analysis/PC/spBallisticCpuDraw.h), `4B9830/4BA910` | Particle records, время, камера, node scale, scratch capacities | Центры/billboard basis, четыре вершины и шесть индексов на частицу, batches | [CPU-геометрия частиц](../effects/ballistic-cpu-draw.md) |
| [spRenderer](../../../Sparkplug/Code/Sparkplug/spRenderer.cpp), [queue math](../../../Sparkplug/Analysis/PC/spRendererQueueMath.h) | Renderable, support, камера, priority bias | Borrowed queue records, distance/material sort keys, порядок callbacks | [Очереди](renderer-protocol.md) |
| [spDXRenderer](../../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp), `4AD540` и lazy getters | Три входные матрицы и dirty flag | Три cached matrix products, сброс dirty | [Matrix cache](renderer-matrices.md), [setters](renderer-matrix-inputs.md) |
| [spAnimTexController](../../../Sparkplug/Code/Sparkplug/spAnimTexController.cpp), [spUVController](../../../Sparkplug/Code/Sparkplug/spUVController.cpp) | Накопленное время, track/evaluator, material holder | Playback, выбранная текстура, UV matrix и её сохранённый snapshot | [Controllers](../materials/material-controllers.md), [UV evaluator](../platforms/uv-functions.md) |
| [spDXRenderer](../../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp), `4BE180/4BB890/4BC410` | Выбранный material, states/overrides, passes/layers | Material block, CPU state caches, подготовленное состояние каждого pass | [Material install](../materials/material-install.md), [geometry submit](renderer-submit.md) |
| [spPCShaderManager](../../../Sparkplug/Code/SparkplugPC/spPCShaderManager.cpp), `4BE310/4C8980` | Vertex components, weights/UV/color, material и light options | Двухсловный ключ, cache lookup; при промахе source/compiled shader | [Ключ](../materials/shader-manager-key.md), [генерация](renderer-generated-draw.md) |
| [spDXShader](../../../Sparkplug/Code/SparkplugDX/spDXShader.cpp), `4AE930` | Parameter descriptors, матрицы, material, light list, UV/palette | Частичные записи в CPU scratch; row/high-water state ведёт следующий renderer-этап `4BE210` | [Shader-константы](../materials/shader-constants.md), [cached draw](renderer-cached-draw.md) |
| [spDXRenderer](../../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp), `4BDE50` | Light list, ambient и CPU caches | Выбранные light slots, упакованный ambient, команды света | [Renderer lights](../effects/renderer-lights.md) |
| [spDXRenderer](../../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp), `4BC4A0/4BC670` | Mesh поля и buffer/declaration identities | Bindings, retain/release, material/pass orchestration перед draw | [Geometry submission](renderer-submit.md) |

## Как связаны эти блоки

Установленная полная ветвь Skin проходит следующую последовательность:

```text
pre callbacks
  → cached Node PRS и inverseBind × boneWorld
  → palette pointer/count и world matrix renderer
  → mesh arguments и buffer bindings
  → material install/states, light preparation при включённой ветви, pass update
  → shader key/cache selection
  → CPU constants → register upload → indexed draw
  → post callbacks и завершение временного состояния
```

Это описание конкретной ветви, а не обязательный порядок всех объектов сцены.
CPU-путь частиц сначала строит собственную геометрию; preselected shader
может обходить автоматический выбор. Общие phases сцены описаны в
[SceneRender](scene-render-runtime.md).

### Матрицы и константы

Setter входной матрицы сохраняет её и помечает cache dirty. Первый lazy
consumer пересчитывает все три cached products; следующие consumers читают
кэш до нового изменения входов. Прямой refresh пересчитывает независимо от
dirty. Поэтому проверять нужно не только значения матриц, но и число/момент
пересчётов и состояние dirty при последовательных чтениях.

Descriptor IDs `1/2/3` читают cached products, ID `4` строит rigid inverse
cached view. Часть световых параметров использует **raw view CA80**;
его нельзя подменять cached product CB00. У палитры и UV различаются и
раскладка, и записываемые lanes: UV оставляет четвёртое слово ряда нетронутым.
Неизвестный descriptor также не обнуляет scratch. Точная таблица всех
22 именованных типов находится в [контракте констант](../materials/shader-constants.md).

### Материал и время

Material installation сначала копирует 17-word fixed-function block,
**затем** вызывает frame/clock gate `4A9530`; controller обновляется при
выполнении его условий. Повторный вызов может пропустить update. Shader getters
позднее читают выбранный material. Эти два потребителя могут видеть разные
состояния одного обновления; объединение их в один «актуальный material snapshot»
меняет поведение.

Controllers потребляют накопленный clock delta. Повторный вызов и shared
controller нельзя трактовать как независимые вычисления: AnimTex хранит
единственный backlink на последний material holder. Для UV нужно сохранять
также snapshot/rebind restoration, а не только итоговую матрицу.

### Очереди, буферы и временные данные

Alpha enqueue читает cached world/view без принудительного world refresh.
Записи очереди borrowed и не продлевают жизнь объектов. Native comparator
не возвращает ноль даже при равных ключах; замена сортировки требует отдельного
контракта tie order.

Bindings буферов имеют другой lifetime: renderer меняет свои retained edges
после device call. Ошибка HRESULT не отменяет cache/refcount update. Declaration
остаётся borrowed; изменение одного stride не заставляет повторить vertex
binding при прежней identity. Эти правила нельзя заменить общим правилом
«каждый pass заново получает все ресурсы».

Palette pointer/count, CPU particle scratch и constant scratch — разные
временные данные. Копирование для внешнего backend должно завершаться в пределах
их действительной жизни и сохранять частично записанные области. Успешный
host callback не устанавливает native lifetime за пределами этого вызова.

## PS2 и численные границы

На PS2 EE также готовит mesh layout, UV matrices, pipeline selection,
scalar parameters и DMA/VIF packets. Это отдельная реализация:
[карта границ](renderer-boundary.md) перечисляет установленные участки.
PC offsets, rounding и layout на неё автоматически не переносятся.
Для `2076E0` отдельно восстановлены [scalar producer и его общий исходник](ps2-cpu-preparation.md):
два выходных потока, ранний NULL, arithmetic operands, накопление счётчика
и безусловное зеркало при возврате.

У PC важны x87 intermediates: для цвета частиц уже известен вход, где
обычный `double` меняет один byte перед усечением. Отдельный
[integer64-significand color helper](../../../Sparkplug/Analysis/PC/spBallisticColor.h)
сохраняет нужную точность в проверенном finite domain; packer получает исходные
clock/birth/lifetime через явный provider. Это не универсальный x87 backend.
На PS2 scalar instruction order можно
восстановить раньше, чем точное поведение EE FPU на zero/overflow и крайних
значениях. Эти ограничения должны оставаться частью каждого алгоритма.

Переносимые finite/bounds guards защищают host; они не доказывают, что
оригинал отклонял те же входы. C++-модели выражений vertex shader учитываются
как shader semantics, а не как CPU-подготовка оригинальной игры.

## Что остаётся открытым

Известные операции не закрывают renderer целиком. Отдельно требуют разбора
остальные producer-to-draw связи, полный lifecycle allocation/upload/reset,
special/offscreen passes, часть texture families, алгоритмы производных particle modifiers
и точные численные края. Для PS2 дополнительно открыты VU semantics и весь
маршрут исполнения пакетов. Проверка API, сравнение CPU bytes и итоговый кадр
подтверждают разные свойства.
