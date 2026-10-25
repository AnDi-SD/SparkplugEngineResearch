# Граница CPU-подготовки и команд графическому устройству

Renderer оригинальной игры выполняет вычисления до передачи команд устройству.
Конечные вызовы Direct3D на PC и формирование DMA/VIF-команд на PS2 не заменяют
подготовку сцены, матриц, материалов, света и геометрии. Извлечение shader-кода
описывает только часть этого пути.

## PC: что происходит до Direct3D

Общая последовательность: CPU-логика сцены → CPU-подготовка renderer → вызовы
Direct3D 9 → обработка runtime/драйвером и устройством. Разделение по
ответственности важно: visibility и симуляция частиц находятся выше renderer,
но renderer также содержит собственную математику.

| Область | Работа до отправки команд | Установленная граница |
| --- | --- | --- |
| Матрицы renderer | `4AD540` вычисляет три произведения 4×4; lazy getters учитывают dirty flag | [Кэш матриц](renderer-matrices.md); входные setters отдельно вызывают `SetTransform` |
| Shader-константы | `4AE930`: rigid inverse, транспонирование палитры, преобразование света в view space, произведения RGBA, `cos(angle/2)`, раскладка UV | [Константы](../materials/shader-constants.md) готовятся на CPU, затем загружаются регистрами |
| Выбор программы | `4BE310` строит ключ; `4C8980` ищет shader в кэше; при промахе составляется исходник и вызывается compiler/assembler | [Генерация](renderer-generated-draw.md) предшествует созданию device shader; компиляция относится к пути промаха, а не к каждому draw |
| Материал и passes | `4BE180`, `4BB890`, `45F570`, `4BC410`: выбор материала, обновление controllers/layers, состояния и обход passes | [Полная отправка геометрии](renderer-submit.md), [controllers](../materials/material-controllers.md) |
| Свет renderer | `4BDE50` группирует источники, готовит payload, упаковывает ambient и обновляет кэши | Затем `SetLight`, `LightEnable`, `SetRenderState`; [свет renderer](../effects/renderer-lights.md) |
| Буферы и текстуры | Сравнение identity, retain/release, извлечение mesh-полей, выбор texture family и подавление повторных bindings | Затем `SetVertexDeclaration`, `SetIndices`, `SetStreamSource`, `SetTexture`; [submit](renderer-submit.md), [pass binding](../materials/material-pass-binding.md) |
| Очередь прозрачности | `454C30/454800`: преобразование центров и расчёт ключа расстояния; сортировка очереди | CPU-порядок отправки; [протокол renderer](renderer-protocol.md) |

В кэше матриц сохраняется установленный порядок: `CB40 = CA80 × CAC0`,
`CB00 = CA40 × CA80`, `CB80 = CA40 × CB40(new)`. Его нельзя менять по
предположению о привычных именах world/view/projection.

Работа **выше** renderer также не сводится к отправке команд:

- [Visibility](../visibility/visibility-runtime.md) отбирает объекты для сцены.
- [Skin](../animation/skin-render.md) формирует палитру костей; применение
  весов к вершинам — отдельная [shader-операция](../animation/skin-deformation.md).
- [Частицы](../effects/ballistic-cpu-draw.md) имеют CPU-путь: `4B9830/4BA910`
  строят по четыре вершины и шесть индексов на частицу перед upload `4BC9A0`.
  Этот путь нельзя заменить одними извлечёнными vertex shaders.

## Где действительно начинаются вызовы API

В конечной части `4BC290` выбранные shaders и подготовленные константы
передаются в device, затем выбирается indexed/nonindexed draw:

| Device slot | Direct3D 9 |
| --- | --- |
| `+170`, `+178` | `SetVertexShader`, `SetVertexShaderConstantF` |
| `+1AC`, `+1B4` | `SetPixelShader`, `SetPixelShaderConstantF` |
| `+144`, `+148` | `DrawPrimitive`, `DrawIndexedPrimitive` |

Точный порядок кэширования, pixel enable gate и аргументы описаны в
[конечном draw](renderer-draw.md). Даже этот участок содержит сравнения
identity и выбор ветви. Более узкие `4CA030/4C9EB0` — тонкие обёртки над
`CreateVertexShader/CreatePixelShader`: compilation происходит раньше.

Вызов Direct3D сам по себе не устанавливает физическое место vertex processing.
Direct3D 9 поддерживает software, hardware и mixed processing; режим задаётся
при создании device, а mixed допускает переключение. Это описано в
[документации Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3d9/processing-vertex-data).
В исходном `4BCCF0` теперь установлены и представлены в C++ ветви выбора
hardware/software processing, REF и NVPerfHUD; подробности находятся в
[device lifecycle](renderer-present-clear.md). Этот body не выбирает mixed.
Конкретная конфигурация работающей игры и возможные дальнейшие переключения
режима ещё не измерены; общего утверждения о выполнении всех вершин на GPU
эти ветви не дают.

## PS2: подготовка EE перед DMA/VIF

На PS2 Direct3D отсутствует. В установленном пути EE готовит данные и команды
для дальнейшей обработки DMA/VIF и VU; GIF/GS образуют отдельную границу.

| Участок EE | Подготовка и вычисления |
| --- | --- |
| `160D10` | Нормализация mesh layout, selector, число streams и компонентов до packet builder `15F900`; [mesh data](../../reference/classes/sp-ps2-mesh-data.md) |
| `1FAB00 → 108EF0` | Построение временной матрицы 4×4 из девяти UV-коэффициентов перед backend-вызовом |
| `2074D0` | Выбор microprogram из таблицы, вычитания, сдвиги и `DIVU` при расчёте размера/ёмкости |
| `2076E0` | [Scalar producer](ps2-cpu-preparation.md): `-1/x`, четыре произведения `255*x`, два выходных потока и счётчик; семантические имена полей пока не установлены |
| `208820` | Вычитание, деление, float/integer conversions и последующая арифметика по элементам; полная семантика алгоритма открыта |
| `2073F0` | Проверка upload-кэша, формирование DMA CALL/MSCAL и расчёт qword lengths |

Четыре извлечённых [VU upload-образа](../../reference/shaders/ps2-vu.md)
не закрывают семантику VU-программ, unit/channel routing, все upload routes
или полный путь до GS. Наличие команд запуска не доказывает их исполнение.

## Значение для замены renderer

При подключении Remix CPU-логика оригинальной игры остаётся источником
подготовленных данных. Граница замены должна сохранять матрицы, палитры,
анимацию материалов, готовую CPU-геометрию, порядок passes и владение ресурсами.
Затем адаптер переводит согласованные данные в команды нового вывода.
Собственные вычисления адаптера учитываются отдельно от CPU renderer оригинала.

`spFixedShaderSkinning` и `spFixedShaderLighting` — C++-модели выражений
шейдера для анализа и потребителей. Их существование не доказывает CPU-исполнение
этих выражений в оригинальной игре.

Эта карта устанавливает границы известных путей и опровергает передачу
«без вычислений». Она не является закрытием каждого метода renderer:
полный allocation/upload/reset lifecycle, некоторые texture families,
special/offscreen passes и все вызывающие ветви требуют отдельных контрактов.
API submission, численная эквивалентность shader и игровая/визуальная проверка
остаются разными уровнями подтверждения.
