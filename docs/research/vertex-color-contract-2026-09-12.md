# Цвет вершин до draw: PC/PS2 layout и PC materialization

12 сентября 2026. Продолжение RTX-исследования по разрешению пользователя
с обязательным дополнением общих восстановленных исходников.

Исправлена доказанная ошибка `spVertexBuffer`: группы весов и UV заканчиваются
на первом отсутствующем бите; повторный расчёт сохраняет offsets отсутствующих
полей. Полная PC-загрузка пяти цветных мешей побайтово совпала с общим C++.
Это уточнение исходников и границы передачи цвета, а не исправление изображения
Remix. Причина текущего затемнения этим экспериментом не установлена.

## Оригинальное поведение и ошибка реконструкции

| Платформа | Оригинальная функция | SHA256 исполняемого файла |
| --- | --- | --- |
| PC | `0045FEA0`, диапазон до `0045FFF0` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| PS2 | `0015C8E0`, диапазон до `0015CB30` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |

В обоих оригиналах stride начинается с 12 байт, число компонент — с трёх.
Группы `0x2..0x10` и `0x800..0x40000` читаются последовательно до первого
нуля. Независимые поля проверяются отдельно. Raw mask не изменяется.
Перестроение не обнуляет таблицу из 22 offsets: оно записывает только поля,
которые действительно вошли в новый layout. Начальная нулевая таблица
принадлежит конструктору, а не каждому перестроению.

Прежний C++ проверял все биты обеих групп независимо и всегда обнулял offsets.
Для свежего `0x2104` оригинал получает stride 16, COLOR0 по смещению 12 байт;
старый C++ — stride 28 и цвет по смещению 16 байт. После `0x940 → 0x2408`
оригинал сохраняет старые offsets normal/color/UV0, но новый stride равен 24.
Маски с разрывами здесь — входы оригинального layout consumer, а не утверждение,
что соответствующие FVF/declaration являются корректным форматом для GPU.
Построитель shader key отдельно ищет старший установленный бит; его нельзя
«исправлять» по правилу CPU layout.

Изменена единственная общая реализация
`Sparkplug/Code/Sparkplug/spVertexBuffer.cpp`; контракт offsets уточнён в header.
`ReadForAnalysis` использует тот же расчёт для ограничения размера входа.
Свежий deep copy по-прежнему создаёт собственный layout; отсутствующие старые
offsets не являются частью копируемого payload. Никаких исключений по имени
уровня, хешу меша или текстуре не добавлено.

`research/probe_vertex_layout_contract.py` выполняет 13 последовательностей,
18 полных оригинальных вызовов на каждой платформе. Сравниваются stride,
число компонент и все 22 offsets — 432 значения на платформу. Старый бинарник
сохранён: **8/18 несовпадений PC и 8/18 PS2**. После исправления — **18/18
точных совпадений на каждой**, результаты PC и PS2 одинаковы.
Повторная инициализация передаёт только результат предыдущего завершённого
вызова. Запись вне подтверждённых layout-полей приводит к отказу проверки.

PC: 51–214 инструкций на вызов, прежние пределы 100k/2s и arena 64 KiB.
PS2: 60–308 инструкций, свежий scalar guest на вызов, пределы 2000/100ms.
Эта PS2-функция не использует VU/MMI/SQ/LQ; proof не распространяется на
PS2 packet materialization. Родитель каждого probe ограничивает процесс 30 s.
Никаких подмен внутренних функций или возобновления оборванного кода.

## Что происходит с цветом при PC-загрузке

`research/probe_pc_vertex_color_materialization.py` исполняет оригинальную цепь
`429A40 → 45FB80/460300 → 45FEA0 → 4AA000 → 4B21E0 → 4AE0E0`
до штатного возврата. Вход файла, allocation, готовое backing renderer и COM
storage/declaration — явно заданные границы стенда. GPU не вызывается.
Все выделенные original objects и COM references освобождаются.

Обычный VB копируется целиком. При `0x20` четыре байта индексов костей
становятся четырьмя **ненормализованными** float; stride увеличивается на 12.
Остальные байты — веса, normal, packed COLOR0 с alpha, UV — сохраняются.
Нормаль `(0,2,0)` не нормализуется во время этой загрузки. Цвет не становится
белым и не умножается на свет. Native declaration задаёт `D3DCOLOR/COLOR0`
по правильному смещению после расширения; это наблюдение настоящего emitter.

| Вход | Маска | Stride CPU → DX | COLOR0 в DX | Сравнение с общим C++ |
| --- | ---: | ---: | ---: | --- |
| Triangle, UV0 | `940` | 36 → 36 | 24 | точно |
| Triangle, UV0/UV1 | `1940` | 44 → 44 | 24 | точно |
| Four weights, UV0 | `97E` | 56 → 68 | 56 | точно |
| Four weights, UV0/UV1 | `197E` | 64 → 76 | 56 | точно |
| Оригинальный `Menus/logo_screen.smo`, field1 | `940` | 36 → 36 | 24 | точно |

Четыре синтетических входа содержат разные RGB и alpha 0/128/255:
`00336699`, `8012A5EF`, `FFE08020`. Planning header намеренно противоречит
payload; оригинальный reader действительно определяет геометрию по IB/VB.
Настоящий SMO проверен по SHA256
`DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7`;
его четыре цвета белые. Это реальный mesh field, не загрузка всего уровня.
Результат: **5/5**, совпали весь VB/IB и FVF/stride/размеры/weight count.
Original helper требует 26 586–27 362 инструкций на случай.

Source probe вызывает существующий `spDXMeshDataSerializer` с PC context и
общий `spDXMesh`, не дополнительный reader в Python. Комментарий о сохранении
цвета добавлен непосредственно в `spDXMesh::BuildVertexBytesForAnalysis`.
Проверен standalone путь. Старая особенность cached byte size packed combiner
из [исследования общего mesh core](tool-render-mesh-shared-core-2026-09-09.md)
здесь не изменялась и не объявляется устранённой.

## Роль материала и последствия для Remix

Повторные original/source проверки `compare_pc_material_lighting.py modes`
и `sources` совпали: 13 переходов lighting и 18 вызовов выбора color source,
включая промежуточные cache/color состояния и порядок команд. Это повторная
проверка [CP32](native-pc-material-lighting.md), а не новый восстановленный
алгоритм. [CP33](native-pc-material-install.md) отдельно сохраняет доказанный
порядок: renderer копирует материал **до** обновления animated color.

В FFP mode2 отключает lighting и выбирает vertex diffuse; mode4 выбирает
vertex ambient; mode5 выбирает vertex diffuse при включённом lighting.
В программируемом `Fixed.rfx` mode4 явно складывает lighting RGB и vertex RGB,
а mode5 перемножает их. Поэтому один и тот же packed цвет нельзя всегда
считать только albedo, только emission или заранее удаляемым освещением.
Эти FFP-state и HLSL правила не являются доказательством одинаковых GPU-пикселей
обоих путей. Наличие COLOR0 само по себе не раскрывает замысел автора меша
или происхождение запечённого света.

Связь настоящего key с исполняемым shader уже описана
[в предыдущем RTX-блоке](winx-remix-shader-key-2026-09-12.md).
Следующая граница универсальной прослойки теперь конкретнее: учитывать выбранный
материал/режим при разделении surface color и добавочной составляющей, отдельно
проверяя FFP и Fixed. Whitening vertex color без сохранения mode4 contribution
потеряет часть исходного результата. Формат передачи этих независимых составляющих
в stock Remix и сравнение на GPU пока остаются открытыми.

## Проверки, учёт и воспроизведение

Основные C++ suites: **6/6** (Engine, shared mesh, render states, full loader,
vertex declaration, mesh reader). Mesh reader — **350/350** assertions,
включая новые original-derived regression cases.
Общее приложение использует эти классы через `SparkplugViewerNative`: DLL
пересобрана, отдельные MeshReader/BufferInspection/RendererSubmit suites —
**3/3**, 4,14 s. Старые C4756 warnings из `spVertexBounds.h` остались.
Те же пять входов дополнительно прошли через её публичный tools ABI:
**5/5**, включая оригинальное поле SMO. Сопоставлены реальные metadata,
indices, positions, normal, RGB/alpha, UV0/UV1, веса и индексы костей.
Это проверка операций общего ядра инструментов; UI/GPU в ней не участвовали.

Готовность RTX сохраняется **44,25% (≈45%)**, материалы 25%, шейдеры 30%:
новое исследование не считается визуальным исправлением. Исследовательские
оценки spVertexBuffer также сохраняются: PC 75, PS2 80; добавлены evidence и
исправление старой реконструкции, без искусственного увеличения процентов.
Игра не запускалась; установленные Remix DLL и настройки не менялись.

Локальные входы, старый/новый source probe, результаты и disassembly:
`local-data/results/vertex-color-contract-20260912/`.
Машинная опись с hash-связями:
`research/vertex-color-contract-2026-09-12.json`.
Датированные прежние отчёты сохранены без переписывания.

```powershell
python research/probe_vertex_layout_contract.py --platform pc --source PATH_TO_SparkplugMeshReaderTests.exe --output local-data/results/FRESH-PC.json
python research/probe_vertex_layout_contract.py --platform ps2 --source PATH_TO_SparkplugMeshReaderTests.exe --output local-data/results/FRESH-PS2.json
python research/probe_pc_vertex_color_materialization.py --source PATH_TO_SparkplugMeshReaderTests.exe --output local-data/results/FRESH-COLOR.json
# Для того же сравнения через общий tools ABI добавить:
# --bridge artifacts/native/viewer/Release/SparkplugViewerNative.dll
```
