# SmoViewer: подробное руководство

## Интерфейс 0.5.0

Левая колонка переключается четырьмя кнопками: **Ресурсы**, **Слои**,
**Анимации** и **Игра**. Панели заменяют друг друга и не перекрывают viewport. В режиме
ресурсов отображается ленивое дерево object graph. Выбор записи подсвечивает связанные
mesh/material/texture и геометрию; двойной клик кадрирует камеру. ЛКМ по геометрии
раскрывает и выбирает соответствующий `spMeshData` в дереве. Нижняя панель хранит
многострочный журнал загрузки и диагностики.

Флажок **«Показывать восстановленные поля»** в режиме ресурсов открывает под
деревом read-only панель собственной serializer-секции выбранного объекта.
Для подтверждённых `spNode`, `spFog`, `spMeshBV`, `spPartitionRenderable`, `spPartitionNode`, `spOctreeNode`, `spBSPNode`, `spOcclusionVolume`, `spMeshNavigationSet`, `spPartitionSystem`, `spZone`, `spZonePortal`, `spZonePortalNode`, `spOBBBV`, `spBoxBV`, `spSphereBV`, `spUVController`, `spLightData`, остальных navigation и particle classes
она показывает номер и имя поля, декодированное значение, ожидаемый layout,
физическое смещение и hex-preview. Составные payload без полностью доказанной
структуры остаются hex-only; панель ничего не записывает в SMO.

Для `spMeshNavigationSet` панель строго декодирует три секции и показывает
`NodeCount`, размеры/histogram/preview node и portal routing matrices, ordered
списки соседей, reciprocity, portal relationships, `Enabled` и связанный
`spMeshBV`. Значение 3 распознаётся как terminal/unreachable marker только вне
допустимого диапазона рёбер текущего узла; при степени 4 это обычный selector.

Верхние кнопки **Экспорт…** и **Импорт…** запускают отдельные приложения
SmoExporter и SmoImporter с уже открытой моделью. Viewer остаётся просмотрщиком и
не встраивает операции изменения формата в свой процесс.

Некоторые уровни используют ссылочные экземпляры общей геометрии. Viewer
распознаёт и разворачивает их для просмотра, а SmoExporter 0.5.0 предлагает явный
выбор: сохранить общие mesh-инстансы в GLB/FBX либо запечь каждое размещение.
OBJ не имеет штатных ссылок на геометрию и всегда разворачивает экземпляры с
предупреждением.

Вкладка **Слои** содержит настройки вспомогательной визуализации. Всё её содержимое
прокручивается единым вертикальным ползунком, поэтому список костей, служебные объекты
и нижние настройки доступны и в компактном окне:

- начальная поза скелета берётся из загруженных Node; Skin использует собственные inverse-bind matrices;
- точки привязки (`hair_top_*`, `hair_bottom_*`, `attach`/`socket`/`locator`/`hook`)
  показываются отдельно;
- выпадающий список **«Сглаживание OpenGL»** переключает `Выкл.`, `MSAA 2×`,
  `MSAA 4×` и `MSAA 8×` без перезагрузки сцены; по умолчанию включён `MSAA 4×`,
  а при аппаратном ограничении Viewer сообщает фактически доступное число выборок;
- диагностический переключатель **«Поворот модели»** вращает model root и служит
  turntable для осмотра геометрии под разными углами;
- выбор глобальной кости показывает все локальные `spSkin[slot]`, где она встречается;
- mesh подсвечивается только если его вершины имеют ненулевой вес выбранной кости.
- collision volumes показываются зелёными ориентированными каркасами, выбранный
  объём выделяется утолщённым красным каркасом;
- control/IK rig, movement tracker и служебные markers включаются независимо;
- объекты без подтверждённой геометрии, включая `Ambient01`, перечисляются в панели.

Выбор поддерживаемого пространственного объекта подсвечивает его в viewport.
Двойной клик в дереве кадрирует mesh, кость, attachment, collision volume,
control/IK node или marker; информационные объекты без transform остаются только
выделенными в дереве и списке.

Вкладка **Анимации** открывает отдельный список клипов. При загрузке SMO viewer
автоматически читает `.anm` и `.san` рядом с моделью. Дополнительно можно добавить
один или несколько SAN/ANM либо выбрать другую папку. ANM используется как карта
состояний и подписей, SAN содержит фактические transform tracks. Выбор клипа
показывает снизу timeline с ползунком, play/pause и переходом на предыдущий или
следующий ключевой кадр. Поза применяется к skeleton/attachments и skinned mesh
через inverse-bind matrices; palette и model matrices обновляются прямо на GPU.

Раскрываемый фильтр **Наборы анимаций** строится по фактическим ссылкам из ANM.
Галки `Bloom`, `AdvBloom`, `Bird` и другие независимо скрывают/показывают
связанные SAN; один клип может принадлежать нескольким наборам. SAN без ANM-ссылки
попадает в отдельную группу `<папка> · без ANM`. Кнопки «Все» и «Ничего» быстро
переключают весь список. Выбор папки сканирует также вложенные каталоги.

Вкладка **Игра** использует встроенный модуль `SmoNativeValidator.Core`. Он находит
`WinxClub.exe` рядом с `Media` открытой модели, через сохранённый ручной выбор или
32-битный registry profile, находит внутренние loader-вызовы по masked-сигнатурам и запускает принадлежащий
validator процесс игры под debugger. Хеш executable используется только в
диагностике и не ограничивает patched-сборки.

Вкладка разделена на две секции: **1. Патчи** и **2. Проверка модели в игре**.
Настройки запуска проверки изначально свёрнуты, а ход нативной загрузки находится
в той же общей прокручиваемой странице и уезжает вместе с остальным содержимым.

В секции **Патчи** находится кнопка **Отключить волосы Блум**. Она запускает
отдельный `WinxHairPatcher.Gui.exe` 0.2.0 из полного suite-пакета; патчер сам
проверяет `WinxClub.exe`, создаёт резервную копию и синхронно настраивает внешние
волосы игрового персонажа и preview-модели меню костюмов. Viewer не изменяет EXE
самостоятельно и показывает предупреждение, если отдельный инструмент не найден.
Если Viewer уже нашёл `WinxClub.exe`, его полный путь передаётся патчеру отдельным
аргументом и подставляется автоматически; внутри патчера можно выбрать другой EXE.

По умолчанию выбрана **быстрая проверка**: выбранная модель подменяет ранний
служебный запрос `Menus\mousecursor.smo`, поэтому нативный loader начинает тест
через несколько секунд без ручного прохождения меню. Это smoke-test контейнера,
геометрии, текстур и создания native resource, но не доказательство работы
скриптов/анимаций в исходной сцене. **Контекстная проверка** ждёт настоящий путь
модели и умеет запустить игру сразу с выбранным `startLevel`; она медленнее, зато
сохраняет игровой контекст.
CLI может дополнительно потребовать `--require-scene-ready`. Тогда ненулевого
`ResourceLoad` недостаточно: validator читает `wxGameFlowController` процесса,
ждёт совпадение текущего и активного state с запрошенным `startLevel` и нулевой
pending state, а затем начинает окно стабильности. Результат сохраняется как
checkpoint `SCENE01` и поле `sceneReadyReached`. Текстовый `GameStateLog.txt`
для этого не используется, потому что игра записывает его с задержкой.

Оба режима используют отдельную временную рабочую папку, частный оконный
`winx.ini` (`fullScreen=false`) и копию `Shaders`. Пользовательские INI, файлы
установленной игры, executable, `Media` и `MediaPath` в реестре не изменяются.
В изолированном дочернем процессе пути чтения переводятся на `Media` рядом с
выбранным executable, поэтому registry другой установки не загрязняет contextual
результат.

Панель отдельно показывает исходный путь модели и фактический trigger, выбранный
маршрут, `startLevel`, длительность и подтверждённые semantic checkpoints: запрос и подмену пути,
вход/возврат high-level resource loader, принятие `FFPS` magic и serializer version,
опциональный `SCENE01`,
second-chance exception, timeout и ограниченный результат `LOAD_ACCEPTED` /
`RUNTIME_STABLE`. Успех требует target-scoped принятия FFPS magic/version;
ненулевой loader result без такого подтверждения помечается `Inconclusive`.
После завершения крупная цветная карточка явно различает пройденный тест,
доказанный отказ модели, незавершённую проверку и ошибку запуска. В карточке
остаётся только последняя подтверждённая стадия (`FFPS03`, `ResourceLoad`,
node/attachment и т. п.); адреса, регистры и полный trace доступны по кнопке
**Открыть полный лог**.
Это пока не per-object trace: адрес generic serializer loop ещё не восстановлен.
Полный JSONL-журнал каждого сеанса сохраняется в
`%LOCALAPPDATA%\SparkplugEngineResearch\SmoNativeValidator\Logs`.

Нативная проверка является встроенной возможностью Viewer. Её переиспользуемое
ядро, исследовательский CLI и тесты хранятся в репозитории как исходный код, но
отдельный пользовательский пакет `SmoNativeValidator` не выпускается.

При открытии новой модели список клипов и состояние групп полностью очищаются.

## Инвентаризация классов

Команда Inspector строит воспроизводимую сводку class ID по одному SMO или всему
каталогу и завершается ошибкой, если встретился незарегистрированный класс:

```text
SmoViewer.Inspect class-inventory <file.smo|directory> [--json]
```

Для каждого класса выводятся hash, engine name, число объектов и файлов и
примеры имён. Текущий PC-корпус `Media` содержит 36 классов в 416 SMO; все 36
сопоставлены с регистрациями `WinxClub.exe`.
Если рядом с выбранным SMO нет ни SAN, ни ANM, viewer находит каталог игры
`Media` относительно пути модели и подключает `Media/Characters/Bloom` как
стандартный набор анимаций. Конкретное имя корневой папки игры значения не имеет;
источник явно показывается в панели и журнале.

Чтобы не перечитывать весь корпус для каждого исследования, Inspector умеет
создавать и инкрементально обновлять локальную SQLite-базу:

```text
SmoViewer.Inspect corpus-db update <database.sqlite> <file.smo|directory> [--json]
SmoViewer.Inspect corpus-db summary <database.sqlite> [--json]
SmoViewer.Inspect corpus-db classes <database.sqlite> [--json]
SmoViewer.Inspect corpus-db metrics <database.sqlite> [--json]
```

Это совместимые команды однокорпусной схемы. Для основной платформенной
schema v5, нескольких PC/PS2-корпусов, всех файлов игры и SMO непосредственно
внутри PS2 PCK служат:

```text
SmoViewer.Inspect pck-inventory <directory> [--parse-smo] [--json]
SmoViewer.Inspect research-db update-directory <db> <corpus> <pc|ps2> <provenance> <directory> <executable> [--json]
SmoViewer.Inspect research-db update-pck <db> <corpus> <pc|ps2> <provenance> <pck-directory> <executable> [--json]
SmoViewer.Inspect research-db summary <db> [--json]
SmoViewer.Inspect research-db sources <db> [--json]
SmoViewer.Inspect research-db formats <db> [--json]
SmoViewer.Inspect research-db format <db> <format-key> [--json]
SmoViewer.Inspect research-db resource-audit <db> [--json]
SmoViewer.Inspect research-db resource-errors <db> [--json]
SmoViewer.Inspect research-db headers <db> [--json]
SmoViewer.Inspect research-db classes <db> [--json]
SmoViewer.Inspect research-db class <db> <class-name|0xhash> [--json]
SmoViewer.Inspect research-db analyze-class <db> <class-name|0xhash> [--json]
SmoViewer.Inspect research-db analyze-all <db> [--json]
SmoViewer.Inspect research-db resources <db> <path-substring> [--json]
SmoViewer.Inspect research-db conflicts <db> [--json]
SmoViewer.Inspect research-db compare <db> <left-corpus> <right-corpus> [--json]
SmoViewer.Inspect research-db integrity <db> [--json]
SmoViewer.Inspect research-db import-evidence <db> <evidence.json> [--json]
```

Дерево использует предметные названия `Model`, `Palette`, `Mesh`, `Material`,
`Texture`, `Bone`, `Attachment`, `Collision`, `Control` и `Marker`, сортируя
связанные ресурсы в порядке рендера.

Поддерживаются подтверждённые node/static-object transforms, полный
наблюдаемый read-only layout material passes, material colors, vertex diffuse
modulation и чтение настоящих PC texture tracks. Точное воспроизведение всех native material
state в viewport, полный gameplay graph SPT/SPL и PS2-варианты SAN пока не
восстановлены.

## Запуск просмотрщика

```powershell
dotnet run --project SmoViewer/SmoViewer.csproj
```

Кнопка **Открыть SMO** позволяет выбрать один или несколько файлов. Файлы,
выбранные одновременно, образуют одну сцену; следующий подтверждённый выбор
полностью заменяет её, а отмена диалога оставляет текущую сцену без изменений.
Навигация повторяет основные жесты Blender:

- `СКМ` — вращение вокруг точки интереса;
- `Shift+СКМ` — панорамирование;
- `Ctrl+СКМ` или колесо — приближение и отдаление;
- `Ctrl+Shift+СКМ` — dolly вместе с точкой интереса;
- стрелки — перемещение камеры в плоскости экрана, `Shift` ускоряет шаг;
- `Home` или `NumPad .` — показать всю сцену;
- `NumPad 1/3/7` — вид спереди, справа и сверху; с `Ctrl` — противоположный;
- `NumPad 2/4/6/8` — шаговое вращение; с `Ctrl` — панорамирование;
- `NumPad +/-` — масштаб; `NumPad 9` — противоположный вид;
- `ПКМ` или `Esc` во время жеста — отменить его.

## Инспектор формата

```powershell
dotnet run --project SmoViewer.Inspect -- inspect Samples/fish.smo
dotnet run --project SmoViewer.Inspect -- scan Samples
dotnet run --project SmoViewer.Inspect -- inspect Samples/fish.smo --json
```

Для воспроизводимой проверки случайной части большого корпуса можно ограничить
число файлов и зафиксировать seed (остальные SMO при этом не читаются):

```powershell
dotnet run --project SmoViewer.FormatTests -- <corpus> --sample-count 10 --seed 20260808
```

Инспектор выводит поля `FFPS`, классы объектов, количество строго
декодированных/неподдерживаемых мешей, найденные E0/E1-layout, native PS2
sphere/count/format/DMA flags, AABB и диагностику несовпадающих
`SBOO`-сигнатур.

## Текущие границы

GUI-сцены используют тот же FFPS/SMO object graph. Если все mesh строго
декодируются, не менее 80% из них плоские, skin отсутствует, а в именах ветвей
есть GUI-состояния или `GUICollision`, Viewer автоматически открывает скрытую
панель **2D / GUI**. В ней можно выбрать корневой экран, показать одно состояние
`NORMAL`/`HIGHLIGHTED`/`PUSHED`/`DISABLED`, включить hitbox-геометрию и перейти
между ортографическим и обычным обзором. Это позволяет не накладывать друг на
друга все сериализованные экраны и состояния одного меню.

Выбор GUI-объекта в обычном дереве автоматически переключает соответствующую
корневую ветвь и, если оно задано предками, визуальное состояние. Выбранный
объект подсвечивается внутри собранного экрана; двойной клик вписывает весь
экран, а не изолированный mesh. Для `GUICollision` одновременно включается
диагностическое отображение hitbox.

Минимальные node-only ресурсы тоже могут быть 2D-layout. Например,
`gameover.smo` не содержит mesh, material, texture, `spTextNode` или самой строки:
в нём сериализованы только ветви `shadow`/`NORMAL`, их transforms и два листовых
слота `text_text01`/`text_text`. Viewer распознаёт такую структуру как
`RuntimeNodeLayout` и показывает на восстановленных позициях диагностические
карточки с именем и состоянием слота. Карточки не выдаются за настоящий текст
игры: строку, шрифт и размеры должен предоставить runtime.

Для layout `0x0100` (`XYZ + Diffuse ARGB`, без UV) просмотрщик сохраняет средний
diffuse-цвет mesh, поэтому состояния кнопок не теряют всю цветовую информацию.
Рендер `spTextNode`/`spTextRenderable`/`spFont` ещё не реализован. Кроме того,
некоторые меню, включая `igmenu_opt_pc.smo`, вообще не содержат этих text-
классов: именованные узлы `resolution`, `value_resolution` и похожие являются
только runtime-якорями, поэтому Viewer показывает их в дереве, но не выдумывает
отсутствующий текст.

Просмотрщик показывает подтверждённую геометрию triangle list/strip, наблюдаемые PC vertex
layouts, cross-platform/Direct3D BGRA `spTextureData`, UV0, vertex diffuse, material colors и
размещение через node/`spStaticRenderObject` transforms. PS2 metadata inspector
сохраняет палитру, raw mip descriptors и byte extents поддержанного контейнера;
неподтверждённые размеры отдельных mips отмечены как неизвестные. Native texture
swizzle и mesh DMA/VIF пока не преобразуются в bitmap/geometry.
Неподдерживаемые layouts получают явную диагностику.

Основной viewport использует единый OpenGL 3.3 path для rigid- и skinned-сцен
и SAN-анимации. Прежняя имитация texture sequences/base-effect layers отключена
на стороне нового ядра до подключения настоящих material passes. Viewer один раз
загружает исходные вершины, индексы и texture в GPU; UV повторяются sampler-ом,
GPU palette skinning применяет SAN-позу, а fragment shader вычисляет
`texture × interpolated vertex diffuse`. Общие
`spMeshData` не копируются для каждого level placement: один VBO/EBO рисуется с
разными model matrices. Поэтому private triangle-atlas, UV rasterization и
копии окрашенных bitmap в этом path отсутствуют.

WPF остаётся только для UI, невидимой hit-test geometry и плоских runtime-якорей.
Обычная 3D-модель требует исправного OpenGL context и не переключается на второй
renderer. Прозрачные поверхности рисуются после opaque geometry
с общим depth buffer; точные уравнения всех `FinalBlendOp` ещё не восстановлены.
Сглаживание выполняется отдельным multisample framebuffer и разрешается в
framebuffer `GLWpfControl`, поэтому его режим можно менять на лету.

Файл можно открыть сразу при запуске:

```powershell
SmoViewer.exe "Media\Levels\Alfea\Alfea03.smo"
SmoViewer.exe "output\flora-miku.smo" "Media\Characters\Bloom\wfgl.san"
```

Если вторым аргументом передан SAN или ANM, Viewer добавляет его после загрузки
модели, выбирает первый явно переданный клип и сразу запускает playback.

Журнал отдельно показывает время decode, подготовки CPU-side scene metadata и
первой загрузки GPU-ресурсов.

В partitioned-уровнях каталожное вложение serializer intervals не считается
transform-иерархией. Матрица `spStaticRenderObject` является готовой world-
матрицей и завершает цепочку, а поля неизвестных `sector*`/portal-классов не
применяются как node transforms. В `Alfea02.smo` это оставляет baked walls в их
vertex world-coordinates и не складывает центры нескольких вложенных секторов.
Field 2 такого объекта использует engine transpose-basis convention; при scale
он не равен общему математическому inverse.

В `Alfea03.smo` также подтверждены reference-only `spModel`: вместо собственной
`spMeshData` такой объект хранит ссылку на ID общей mesh-геометрии, а его
`spStaticRenderObject` задаёт отдельную world-матрицу. Viewer разворачивает эти
ссылки в собранной сцене, помечает записи дерева как
`Mesh instance → [индекс источника]` и отдельно показывает число физических mesh
и экземпляров. Например, физический mesh `[2674] casierD16` имеет ещё 21
размещение вдоль стен — всего в сцене видно 22 шкафа. Это размещения уровня, а не
22 независимые копии геометрии в файле.

Дополнительно для `Alfea03.smo` восстановлены три ранее ошибочно отмечавшихся
случая: mipmapped Direct3D BGRA texture `[656] top` (Viewer читает полный базовый уровень),
статический двухслойный материал `Pcrystal12` с `crystal2` по UV0 и `cryst_hl`
по UV1, а также штатные rigid glow/spark tuple `FinalBlendOp=6`. Известные
эмиссионные приближения WPF остаются описаны в состоянии материала, но сами по
себе больше не считаются ошибками загрузки уровня.

Для PC character assets также поддержан структурный Direct3D BGRA layout, vertex
layout `0x0800` и повторное использование нескольких равновеликих atlas по
порядку объектов каталога. Это покрывает текстуры одежды, волос и глаз в
`bloom_dating_outfit_02.smo` и `bloom_dating_outfit_03.smo`.

Часть root-level prototype assets в составных уровнях пока нельзя структурно отличить от
baked world geometry. Skin и animation поддержаны только для перечисленных ниже
подтверждённых PC-форматов; остальные material passes, gameplay layer SPT/SPL и
PS2-варианты не восстановлены. Поэтому это исследовательский просмотрщик, а не
финальный рендер сцены из игры.

Viewer больше не трактует `FinalBlendOp` как набор флагов. Операции `0x4`, `0x5`
и `0x6` имеют разные нативные назначения и показываются разными диагностическими
приближениями. Для `0x6` дополнительно учитываются все 11
`MaterialRenderStates`, тип consumer и состояние владеющего `spSkin`.
Подтверждённые прозрачные skinned-поверхности IceWorm/Yeti используют точный
tuple `[0,0,1,2,1,1,3,0,4,0,6]` и `AlphaSortEnable = 1`; проверки только
`MaterialRenderStates[8] = 4` недостаточно. Rigid- и skinned-effect consumers
имеют другие подтверждённые tuple. OpenGL preview не воспроизводит нативный blend pipeline
побайтно, поэтому даже визуально правдоподобный viewport не заменяет проверку
модели в реальной игровой сцене.

У `bloom_princess.smo` есть отдельный подтверждённый transparent-surface профиль
с `FinalBlendOp = 0x2`: tiara mesh `[83]` использует точный tuple
`[0,0,1,0,1,0,3,0,4,0,6]`, владеющий `spSkin [81]` с
`AlphaSortEnable=0`/`Priority=1`, чёрный vertex diffuse и общую текстуру
`b_prince`. Это не правило «любой `0x2` с alpha»: Viewer включает профиль только
после проверки всего consumer-контекста и фактического покрытия UV0 одновременно
нулевого и промежуточного alpha. В эталоне покрыты 3948 texels: 53 с alpha 0, 3746 с
промежуточным alpha и 149 с alpha 255, поэтому это не доказанный бинарный
alpha-test/cutoff. Sibling chunks `[85]`, `[87]`, `[89]` не покрывают alpha-zero
texels и остаются opaque. Классификация влияет только на OpenGL preview и порядок
отрисовки; Viewer не исправляет и не переписывает модель для игры.

В pristine `Minautor.smo` подтверждён второй, отдельный transparent-surface
профиль `FinalBlendOp=0x2`: mesh `[13]` использует tuple
`[0,0,1,0,1,1,3,0,4,0,6]`, skinned surface consumer с
`AlphaSortEnable=1`/`Priority=1`, чёрный vertex diffuse и фактически покрытый
UV0 промежуточный alpha. Viewer классифицирует только полное совпадение как
`SkinnedTransparentSurfaceFinalBlend2`, ставит его в прозрачный порядок и
оставляет обычным `DiffuseMaterial` без emissive/glow. Сочетание princess tuple
`RS[5]=0` с `AlphaSortEnable=1` остаётся видимым, но получает отдельный режим и
предупреждение `UNCONFIRMED_FINAL_BLEND_2_HYBRID`: pristine bound fixture для
такой пары не найден.

Большой partial-alpha run с `RS[5]=0`, сопоставимый по размеру с телом, получает
`LARGE_ALPHA_RS5_ZERO_ORDERING_UNCONFIRMED` и viewport-баннер. Это предупреждает
о риске крыльев/крупных накладок: WPF сортирует только внутри своей изолированной
модельной сцены и не может доказать порядок относительно alpha-tested кустов,
листвы и другой геометрии уровня. Значение `RS[5]` при этом не объявляется
полностью декодированным нативным depth-write switch.

Для generated `imp_o_x_*` runs Viewer также сравнивает uniform vertex diffuse с
сохранёнными skinned-поверхностями тела. Пара `FFFFFFFF` у тела и `FF000000` у
face-run даёт `IMPORTED_FACE_VERTEX_DIFFUSE_MISMATCH` и баннер
**MIXED FACE VERTEX DIFFUSE**. Это факт различия сериализованных данных и повод
проверить модель под разными углами; WPF preview не воспроизводит native
fixed-function lighting и не устанавливает причину игрового кадра.

Для подтверждённого princess-style профиля Viewer дополнительно проверяет гранулярность
одного native draw unit. Если один `spSkin/material/mesh` объединяет много удалённых
друг от друга связных компонентов с разными deform-targets, выводится
`NATIVE_ALPHA_RUN_GRANULARITY_UNCONFIRMED`. Такое объединение может выглядеть
правильно в WPF, но игра сортирует его как одно целое и способна скрыть близкие к
непрозрачному телу глаза, рот или украшения. Диагностика не доказывает конкретный
порядок native renderer; она требует сохранить исходные material/renderable run
boundaries и проверить итоговый кадр в игре.

Для того же узкого princess-style профиля Viewer проверяет ещё один риск: маленький
partial-alpha run, почти лежащий на непрозрачной skinned-поверхности в bind pose.
Кандидат считается рискованным только при одновременном совпадении подтверждённого
профиля, малого размера, расстояния не более 0,1% диагонали opaque-body и почти
параллельных normals как минимум у половины вершин. Тогда выводится
`NATIVE_ALPHA_DECAL_DEPTH_UNCONFIRMED`, а над viewport появляется явная пометка
**NATIVE-RISK DIAGNOSTIC · SIMULATED FACE-OVERLAY LOSS**. Только у найденного run
Viewer временно обнуляет sampled texture alpha: это показывает лежащую под ним
opaque-поверхность, как при полном отклонении face-overlay draw. Это
**SIMULATED FACE-OVERLAY LOSS**,
а не эмуляция native blend/depth pipeline и не доказательство причины игрового
кадра. SMO не изменяется; обычные поверхности, крылья, подвеска, заколка и
эталонная tiara `bloom_princess.smo` именно симуляцией скрытия не затрагиваются.
Большой `RS[5]=0` run может независимо получить только предупреждение о порядке.

Для PC `spSkin` общий reader сохраняет bone references, inverse-bind matrices
и исходное число весов; размер palette берётся из загруженного Skin. Vertex
decoder поддерживает подтверждённые skinned layouts. Жёсткие детали под костями
размещаются через фактический world соответствующего render support — например,
глазные meshes под `Head` и предметы в руках. PC SAN/ANM tracks и динамическая
деформация позы поддержаны для подтверждённого `spAnimation` (`0x56EE563A`).
Общий decoder/sampler читает PC SAN representation 1–4: packed linear,
packed cubic/Squad и три независимые scalar-оси, включая смешанные 3/4 для
вращения. Подготовка коэффициентов и привязка дорожек выполняются при загрузке;
кадр использует бинарный поиск интервала и сохранённые каналы. Поддерживается
неединичный scale, который формат VMD представить не может.

Сэмплер сохраняет подтверждённое PC-поведение двух ключей: на последнем времени
и после него возвращается первое значение. Это может давать резкий переход.
Вращение scalar-осей — радианы, порядок `qZ*qY*qX`. Пустые cubic/scalar,
неизвестные representations, неконечные используемые значения, повторные времена
и незакрытые поля отклоняются с ошибкой; исходные bytes не изменяются.
Ограничения: один PC FFPS 0x26 объект, SAN до 64 МиБ, до 100 000 ключей на ось.
PS2 SAN и выполнение animation events этим decoder не заявлены.

Одноимённые SAN-дорожки с разными PRS-каналами объединяются. При конфликте
одного свойства инструмент использует первое и пишет предупреждение;
это явная политика инструмента, а не восстановленное правило конфликта SAN.
Повреждённый выбранный SAN останавливает предыдущий clip; нулевая длительность
остаётся статической позой. `SmoViewer.GuiTests` проверяет реальные обработчики
открытия, выбора SAN и slider без показа окна и без автоматизации OS-диалогов.
Команда Inspector `animation-bindings <model.smo> <file.san|directory>` отдельно
показывает exact, case-only и отсутствующие привязки имён tracks к узлам SMO.
Команда `animation-coverage <model.smo> <directory>` агрегирует по всем SAN число
clips, tracks и position/rotation/scale keys для каждого узла модели.
Команда `mesh-inventory <file.smo> [--json]` выводит для каждого render mesh
serialized bytes, vertex/index/triangle count, disk/runtime stride, owning skin и
размер его palette.

Нативный PC runtime подтверждает, что lookup регистрозависим: one-byte mutation
`R_Ankle -> r_Ankle` загружается, но не получает ни exact, ни cross-case
совпадения. Поэтому `case-only` в Inspector означает потерянную привязку.
Playback Viewer применяет SAN tracks через точное `Ordinal`-сравнение: потерянная
из-за регистра привязка больше не маскируется preview-режимом.

Дополнительные one-byte tests подтверждают, что missing parent/leaf target и
duplicate toe names в обоих порядках не ломают loader. Missing track остаётся
локальным: новое имя получает отдельный binding slot `0xD8`, а descendant
`foot_right` сохраняет exact slot `0x46`. Duplicate namespace даёт один exact key
и делает противоположное имя missing; два разных evaluator получают общий slot,
поэтому на binding-слое подтверждён all-target. Inspector показывает это как
missing/ambiguous. Отдельной проверкой остаются только final descendant pose и
world transforms в маршруте с активным animation tick.

Для WPF texture × vertex-color modulation создаётся mesh-local tinted atlas.
Вокруг покрытых UV-островов добавляется двухпиксельный color gutter, чтобы
билинейная фильтрация не создавала светлые полосы на швах.

Код прежнего равномерного таймера ещё находится в UI, но новое ядро не выдаёт
ему выдуманные sequences/FrameDuration. Настоящие временные ключи и все passes
доступны в `SmoLoadedMaterial`. Например, BloomX содержит три прохода и два
контроллера по38 end-time keys с10 различными texture IDs. Их точное исполнение
подключается отдельно. Декодированные UV1 сохраняются для будущей реализации
соответствующего многопроходного OpenGL backend.

## Проверки

```powershell
dotnet run --project SmoViewer.FormatTests -- Samples
```

Без локальной папки `Samples` выполняются синтетические тесты контейнера.
С игровым корпусом дополнительно проверяется каждый найденный `.smo`.

## Данные игры

`Samples/` намеренно исключена из Git. Не добавляйте оригинальные или
модифицированные игровые ресурсы в репозиторий.

Двухбайтовые значения `0x0EE3`, `0x32E3`, `0x43E3`, `0x54E3` и `0x29E3`,
которые старый decoder называл texture formats, теперь сохраняются только как
legacy diagnostic signatures: `E3` является field header, а соседний байт —
частью payload size. Настоящий PS2 pixel format хранит значения 0/1/3.

Подтверждённые сведения о формате собраны в
[`docs/SMO_FORMAT.md`](SMO_FORMAT.md).

## Благодарности

Butermix подготовил независимый прототип batch scene export, заготовку UV Editor
и исправление перехода SharpGLTF на `ToGltf2`. Эти материалы были проверены при
проектировании текущих инструментов; подробности и границы переноса зафиксированы
в [благодарностях](../ACKNOWLEDGEMENTS.md).
