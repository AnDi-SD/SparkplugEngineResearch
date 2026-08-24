# Формат SMO

Статус: черновая спецификация, основанная на текущем parser и локальном частично изменённом корпусе. Техническая запись рядом с реализацией находится в [`tools/SmoViewer/docs/SMO_FORMAT.md`](../../tools/SmoViewer/docs/SMO_FORMAT.md).

## Короткий ответ

SMO — это little-endian `FFPS`-контейнер сериализованного объектного графа Sparkplug, а не просто архив одного mesh. В одном файле могут быть модели, несколько мешей, материалы, текстуры, scene nodes, skin/collision и вспомогательные render-объекты. Каталог связывает имена и class ID с сериализованными областями данных; движок восстанавливает из них runtime-объекты.

Последнее предложение подтверждено формой каталога и набором зарегистрированных классов, но точный порядок создания объектов, владение и разрешение ссылок ещё требуют проверки по executable/runtime.

## Заголовок FFPS

Заголовок занимает `0x20` байт:

| Offset | Тип | Текущее значение/смысл |
|---:|---|---|
| `0x00` | `char[4]` | `FFPS` |
| `0x04` | `UInt32` | наблюдалось `0x26` |
| `0x08` | `UInt32` | неизвестно |
| `0x0C` | `UInt32` | объявленный размер файла |
| `0x10` | `UInt32` | вариант; наблюдались `1`, `2`, `3`, `8`, `9` |
| `0x14` | `UInt32` | абсолютный `DataStart` |
| `0x18` | `UInt32` | `DataSize` |
| `0x1C` | `UInt32` | `ObjectCount` |

Для корректного исследованного файла ожидается:

```text
DataStart + DataSize == FileSize == actual file length
```

Общая checksum файла в подтверждённых полях не обнаружена. Игровые RGB-пробы
показали, что данные можно менять без обновления отдельной checksum, если
согласованы размеры/offsets и не нарушен object graph. Поле `0x08` остаётся
неизвестным и не должно называться checksum без отдельного подтверждения.

Варианты `8` и `9` наблюдались у ресурсов с суффиксом `_ps2`. Называть поле `0x10` версией формата пока преждевременно.

## Каталог объектов

Каталог начинается с `0x20`. Запись имеет переменную длину:

```text
UInt32 entryMarker
UInt16 nameByteCount
Byte[nameByteCount] zero-terminated name
UInt32 typeHash
UInt32 logicalOffset
UInt32 serializedSize
```

После последней записи следует нулевой `UInt32`. Адрес тела вычисляется строго:

```text
physicalOffset = DataStart + logicalOffset
```

`serializedSize` может включать поддерево. Интервалы каталога могут не пересекаться или полностью вкладываться друг в друга; вычислять размер как расстояние до следующего logical offset нельзя.

## Тело объекта и data block

Обычное тело начинается так:

```text
UInt32 typeHash
char[4] "SBOO"
... serialized fields ...
```

В первом байте поля `spDataBlockSerializer` младшие пять бит задают field type, старшие три — size code:

| Size code | Размер payload |
|---:|---:|
| `0` | 0 |
| `1` | 1 |
| `2` | 2 |
| `3` | 4 |
| `4` | 8 |
| `5` | следующий `UInt8` |
| `6` | следующий `UInt16` |
| `7` | следующий `UInt32` |

Если пятибитный type равен `0x1F`, настоящий type находится в следующем байте.

Подтверждённые hash/name пары вынесены в [реестр class ID](../reference/class-ids.md). Parser сверяет hash записи каталога с hash в теле и сообщает расхождение, но не перемещает объект эвристически.

## Актуальная проверка PC-корпуса 2026-08-10

`SmoViewer.FormatTests` на `local-data/pc-pristine/Media` успешно обработал 416/416 SMO: 177 369 записей object directory и 22 012 mesh без падения strict parser (140 269 assertions). Это актуальный baseline чистого PC-корпуса. Приведённые ниже числа прежнего смешанного/частично изменённого scan сохранены только как историческая диагностическая выборка и не должны подменять этот baseline.

## Mesh data

Текущий строгий decoder подтверждает:

- варианты сериализации, условно названные `E0` и `E1`;
- primitive type `3` как triangle strip;
- позицию из трёх `Single` с offset `0` для известных layouts;
- для `0x093E` — четыре `Single` blend weights с offset `12` и четыре локальных
  `UInt8` palette indices с offset `28`;
- необходимость различать serialized stride и runtime vertex-buffer stride;
- необходимость определять границы из структуры объекта, а не глобального поиска байтовой сигнатуры.

Известные примеры различия stride:

| Ресурс/layout | Runtime stride | Serialized stride |
|---|---:|---:|
| `fish.smo`, `0x093E` | 56 | 44 |
| `bloom_ball.smo`, `0x197E` | 76 | 64 |
| `loading.smo`, `0x0940` | 36 | 36 |

Не закрыты primitive type `2`, несколько PS2-вариантов `E1`, часть boundary cases и семантика всех vertex layouts.

## Материалы и текстуры

### Render state `spMaterialData`

Первый подтверждённый data-block после сигнатуры материала имеет field type `3`
и payload `UInt32`. Это `FinalBlendOp`, то есть отдельная операция, а не битовая
маска. Значения `0x4`, `0x5` и `0x6` нельзя объединять проверкой бита `0x4`:
`0x4` подтверждено у glow/effect-проходов, а остальные значения требуют проверки
полного material/consumer state.

Production fixture обычной skinned texture-alpha поверхности — нативный comparator
`Minautor.smo`. Его прозрачные triangles находятся в самостоятельных
material-bearing ветвях `spSkin/material/mesh`, которые ссылаются на character
texture, и используют:

- `FinalBlendOp = 2`;
- `MaterialRenderStates = [0,0,1,0,1,1,3,0,4,0,6]`;
- `LayerTextureStates = [0,3,3,0,0,4278190080,2,0,0]`;
- `AlphaSortEnable = 1`, `Priority = 1`;
- vertex diffuse `0xFF000000`.

Каждый самостоятельный source renderable требует собственной material-bearing
ветви. Material-less mesh допустим только как palette/ushort continuation того же
renderable. Тиара `bloom_princess.smo` по-прежнему полезна как структурный пример
малой локальной alpha-ветви, однако её `MaterialRenderStates[5] = 0` и
`AlphaSortEnable = 0` не подходят как production-шаблон для крупных skinned-крыльев:
в игре более далёкие alpha-поверхности листвы и уровня могут ошибочно перекрывать их.

Произвольный `FinalBlendOp = 2` не означает прозрачность. Для такой классификации
нужны точный связанный consumer state и прозрачные texels, фактически покрытые UV
его triangles. Ветви `IceWorm`/`Yeti` с `FinalBlendOp = 6` подтверждают другое
семейство и не дают универсального шаблона. Само наличие неоднородного alpha в
текстуре также ничего не доказывает: например, обычный atlas `knut` содержит
служебные alpha-значения при непрозрачном материале.

У `knutBoss.smo` материал `[10]` mesh `[13]` имеет `FinalBlendOp = 0x6`, а `gr_01`
содержит 211 полностью прозрачных, 746 полупрозрачных и 67 непрозрачных пикселей.
Материал тела `[26]` имеет `FinalBlendOp = 0x2`. Поэтому Viewer и Exporter
определяют режим по полной связке material/consumer state, а не по одному биту
или эвристике по пикселям. OpenGL-просмотр воспроизводит восстановленные
render-state contracts, но сам по себе не доказывает полное совпадение с native
render path.

Rigid `spModel` без skinning может принадлежать анимируемому `spRenderNode`.
Такой mesh сохраняет собственный model world transform в bind pose, но при
воспроизведении/экспорте вычисляет локальный transform относительно ближайшего
render node и следует его SAN-треку. Подтверждённый пример — mesh `[6]` очков
`knut.smo`, связанный с `[2] Knut_TEMP_glasses`.

`SMOTextureTool` даёт независимые инженерные подтверждения:

- подтверждённые PC texture layouts используют BGRA pixel payload;
- текстура находится в графе material/layer/texture objects;
- для корректного preview требуется учитывать vertex diffuse modulation;
- repack без изменений может быть byte-identical;
- writer/repack `SMOTextureTool` не считается совместимым с игрой: его файлы могли проходить внутренний parser, но вызывать crash;
- игровая проверка подтвердила замену только RGB-байтов внутри существующего fixed-size BGRA pixel buffer при сохранении исходного Alpha, headers, offsets и длины файла;
- исторические заявления об игровой проверке texture replacement 1024/2048 считаются опровергнутыми до нового независимого подтверждения.

Эти данные теперь воспроизводятся в основном viewport без промежуточной
запеканки. Viewer загружает positions/indices, UV0/UV1, texture и vertex diffuse
непосредственно в OpenGL buffers; fragment shader выполняет `texture ×
interpolated diffuse`, как подтверждённый `D3DTOP_MODULATE` path игры. Один
физический `spMeshData` имеет один VBO/EBO, а reference-only placements
отличаются model matrix. Skinned mesh деформируются palette matrices в vertex
shader, SAN обновляет bone/model transforms на GPU, texture sequences меняют
кадры без CPU baking, а подтверждённые layered materials используют base UV0 и
effect UV1. GUI-сцены используют orthographic projection. Opaque и transparent
passes, а также сетка пола используют общий depth buffer.

Private triangle-atlas ниже описывает только аварийный WPF fallback при
недоступном OpenGL context. Невидимая WPF geometry сохраняется для hit testing,
но не участвует в видимом кадре. Это ограничение просмотрщика, не часть формата
и не алгоритм игры. Контрольный pristine
`Alfea03.smo` использовал 509 unique mesh buffers, 63 textures и 1 263
placements; подготовка сцены на локальной debug-сборке сократилась с 32,17 до
0,20 с, working set — примерно с 934 до 281 МиБ.

В `Alfea_broken_01.smo` mesh `[1022] Ray_light_F04` подтверждает vertex-only
light без bitmap: mixed white/yellow RGB сочетается с alpha `0/0x65`,
`FinalBlendOp=2` и companion state `2`. Direct GPU path сохраняет этот alpha и
рисует quad после opaque wall. Правило требует нулевого и видимого alpha вместе
с указанным material family, поэтому обычный baked-lighting alpha у opaque
полов не становится прозрачностью. `[1005]` и `[1018]` подтверждают ту же схему.

`[1078] poutreW06-000` при этом texture имеет: shared `marble2 [43]` 64×64.
Она почти белая, а более контрастная `linegen00 [1065]` назначена отдельному
парному mesh `[1082]`; это два физических элемента одной составной балки.

Vertex format `0x1900` со stride 32 хранит `XYZ` по `+0`, diffuse ARGB по
`+12`, UV0 по `+16`, UV1 по `+24`, без normal. Пять таких mesh в pristine
`Alfea_broken_01.smo` — части `WallA-000`; mesh `[4085]` является
vertex-colour-only стеной с 877 вершинами и 303 записанными RGB. До регистрации
layout Viewer отбрасывал эти каналы и показывал стену своим голубым
fallback-цветом. Opaque `FinalBlendOp=0` не использует неоднородный старший байт
этого diffuse stream как прозрачность.

Аудит pristine `Alfea03.smo` добавил ещё один строгий PC texture layout:
`0x0EE3` хранит mip chain, при этом базовый BGRA-уровень использует width
`+0x24`, height `+0x28`, нулевой marker `+0x3C` и pixels `+0x3D`, как
`0x32E3`. Подтверждённый `[656] top` имеет размер 256×256; Viewer проверяет
вложенные размеры `E3:0E`, `E1:20`, `E0:1A` и читает базовый уровень.

Там же материал `[4599] Pcrystal12` подтверждает статическую двухслойную схему:
`[4600] crystal2` семплируется по UV0, `[4601] cryst_hl` — по UV1 mesh `[4603]`,
а `[4602] spUVController` управляет вторым слоем. Это не texture sequence.
Rigid book glow/spark meshes `[273]`, `[278]`, `[294]`, `[298]`, `[313]`,
`[317]` используют `FinalBlendOp=6` и tuple
`[0,0,1,2,1,1,3,0,2,0,6]`; поэтому `RS[3]`/`RS[5]` без consumer geometry не
доказывают skinning. OpenGL показывает их диагностическим effect-приближением,
но это штатный материал уровня, а не ошибка загрузки.

У `[2756] plaque02` правильная texture `[2755] sign_faragonda` имеет 128×64,
а mesh сочетает её с девятью vertex RGB на общих UV. Прямой GPU-path сохраняет
эти входы без преобразования. В WPF fallback требуется private triangle-atlas;
его разрешение выбирается универсально, без проверки назначения
объекта: максимальный UV-размах треугольника умножается на размеры исходной
texture, затем добавляются крайний отсчёт и защитное поле. Несколько из 20
треугольников адресуют почти все 128 texels по горизонтали, поэтому `[2756]`
получает ячейку 133×133 вместо размытой 32×32.

Единый WPF-бюджет ограничивает preview-atlas размером 2048 по стороне и
1 048 576 пикселями. Это явно техническое ограничение просмотрщика, одинаковое
для всех ресурсов, а не семантическое правило SMO и не признак достоверного
разрешения исходной игры при срабатывании лимита. При выборе mesh журнал Viewer
показывает отдельно требуемую и фактически выделенную ячейку atlas.

Однако ранние эксперименты при изменении длины pixel buffer обновляли общие `FileSize`/`DataSize`, но не все последующие записи каталога. Поэтому signature scan полезен как восстановительный инструмент, но не заменяет корректный object parser и catalog-safe repack.

Практический writer находится в `SmoImporter`. Legacy single-texture путь переносит
PNG/JPEG или embedded GLB/FBX base-color в BGRA-блок target. Если исходные размеры
точно представимы полями SMO, они сохраняются без resize; это включает проверенный
вариант `3000×3000`. Непредставимый размер никогда не уменьшается и поднимается до
ближайшего совместимого POT-размера. При изменении длины блока writer пересчитывает
FFPS catalog offsets/sizes, enclosing object sizes и вложенные размеры цепочки
`spSkin → material → TextureData`, затем повторно запускает strict parser и проверку
исходных skin palettes. Generated-skinning
multi-material путь собирает один RGBA-atlas и сохраняет donor Alpha полностью.
Opaque triangles остаются в существующих opaque consumers target, а alpha triangles
получают добавленные `spSkin/material/mesh` branches с общей texture reference.

Нативный trace 2026-08-14 исправил трактовку прежнего RGBA-crash. В форматах
`0x32E3`/`0x43E3` байт `texture + 0x3C` — marker `00`, а BGRA payload начинается
с `+0x3D`. Старый writer начинал на байт раньше и портил marker, из-за чего игра
читала число строк как `0xFF000100` вместо `0x00000100`. Это был off-by-one, а
не доказанный запрет Alpha. Исправленная полная BGRA-запись того же donor Alpha
затем прошла target-scoped FFPS, вернула native resource и пережила окно
наблюдения без исключения. Это подтверждает layout и загрузку BGRA payload, но не
визуальную семантику конкретного blend state.

Игровой тест опроверг достаточность catalog-safe texture repack: вариант
`Faragonda.smo → bloom_jeans.smo`, где две группы `64×64` были объединены в
структурно корректный `128×64` leaf с пересчитанными catalog/object/reference
sizes, проходил strict parser и оба format test, но вызывал crash игры. Значит,
внутри runtime существуют дополнительные ограничения на texture object/layout,
которые ещё не выражены в известных FFPS-полях.

Следующий эксперимент — перенос полного donor render graph и отдельное добавление
известной service-ветви target — также вызвал crash, включая ранее работавшие пары.
Это доказывает, что выделенного набора collision/control objects недостаточно:
неизвестные target bindings должны сохраняться вместе с исходными object IDs.

Поэтому активный SMO → SMO writer сохраняет весь target graph и добавляет полные
visual branches внутри него, не заменяя service/skeleton graph целиком. Отдельный
generated-skinning multi-material path также сохраняет target graph, но использует
один общий RGBA-atlas. PNG Alpha не задаёт native material однозначно: у контрольной
Layla `mat3`/глаза и `mat4`/рот содержат полезный RGB под исходным `A = 0`, но должны
рисоваться как opaque face decals. Им назначается явный source-bound профиль
`OpaqueOverlay`: до premultiplied resize всей выбранной texture group ставится
`A = 255`, а 124 triangles записываются двумя независимыми post-body branches с
каноническим eye state `FinalBlendOp = 0`,
`MaterialRenderStates = [0,0,1,0,1,1,3,0,4,0,6]`,
`LayerTextureStates = [0,3,3,0,0,4278190080,2,0,0]`,
`AlphaSortEnable = 0`, `Priority = 1` и vertex diffuse `0xFFFFFFFF`. Белый diffuse
совпадает с generated retained body для OBJ без vertex colors. Нормали лицевых
накладок не меняются и проходят существующий importer path. Оставшиеся `mat5:34`,
`mat7:34`, `mat6:2` содержат 70 действительно прозрачных triangles и получают три
независимые ветви с
production-state `Minautor.smo`: `FinalBlendOp = 2`,
`MaterialRenderStates = [0,0,1,0,1,1,3,0,4,0,6]`, те же `LayerTextureStates`,
`AlphaSortEnable = 1`, `Priority = 1`, vertex diffuse `0xFF000000`; 2 714 opaque body
triangles остаются на существующих opaque branches. Общая texture reference не
разрешает объединять разные renderables в один `spSkin`; material-less skin допустим
только как palette/ushort continuation того же renderable. Ветви используют текущие
target weights/palettes.

Без явного профиля безопасно вывести эту семантику нельзя: одинаковые PNG Alpha и
геометрическая близость встречаются и у настоящих прозрачных поверхностей, и у
непрозрачных накладок. Текущая `ImportedMaterial` хранит только имя и ссылку на
base-color texture; OBJ-директивы `d`, `Tr`, `map_d` и эквивалентная opacity metadata
в этот контракт не входят. Поэтому default `Auto` не изменён, а для контрольной Layla
в GUI у `mat3`/`mat4` выбирается **Непрозрачная накладка**, тогда как
`mat5`/`mat6`/`mat7` остаются в **Авто**.
Прозрачная подвеска `mat6` остаётся alpha overlay
из двух triangles поверх opaque body branch, а не превращает всё тело в прозрачный
consumer. Strict/Viewer/native проверки подтверждают структуру и загрузку такого
графа, но не native blend, lighting или depth/sort; OpenGL Viewer может скрыть ошибку
объединённого alpha-run, а orbit камеры при фиксированном world-light — углозависимый
дефект материала. Визуальный паритет нового контракта не подтверждён до
пользовательского теста вновь созданного SMO непосредственно в игре.

## Результат текущего scan

На локальном частично модифицированном корпусе строгий decoder обработал `24 039` из `25 395` объектов `spMeshData`. Остальные группы диагностированы как:

| Группа | Количество |
|---|---:|
| stale offset / signature mismatch | 680 |
| PS2 preamble variant | 494 |
| PS2 boundary variant | 137 |
| primitive type `2` | 45 |

Это baseline конкретного корпуса, не статистика всех игр или всех SMO. После получения чистой сборки scan должен быть повторён с manifest.

## Уточнения корпуса 2026-08-10

- `spNode`-иерархия skeleton восстанавливается по `esfNodeChild` (field type `5`), а не по каталожному `ParentIndex`, который описывает владение сериализованными интервалами.
- PC-layouts `0x097E` и `0x197E` хранят четыре `float32`-веса по `+12` и четыре байтовых индекса локальной bone palette по `+28`; `spSkin` отдельно хранит palette и inverse-bind matrices.
- В исследованном PC-корпусе palette имеет 16 slots, в PS2-корпусе — 64. Независимый Kikko rig использует для тела PC palettes `16 + 9`, тогда как PS2 хранит те же 20 уникальных костей в одной palette. Автоматическое разбиение exporter'ом остаётся сильной гипотезой.
- `bloom_jeans.smo` подтверждает распределение одного 95-node skeleton между шестью локальными PC palettes: основной body mesh не содержит arm/hand bones, которые находятся в четырёх дополнительных skin parts. Palette slot не является глобальным bone ID; надёжное объединение выполняется по node object ID.
- PS2 `E1` содержит platform-specific DMA/VIF representation: для 973 mesh Gardenia01 выполняется `payloadSize = 0x28 + dmaQwordCount * 16`; первые четыре `float` задают bounding sphere.
- PS2 `E2` в Gardenia01 имеет длину 24 байта и соответствует `esfMeshDataBoundingBox` (`minXYZ`, `maxXYZ`).
- `menu.smo` — GUI scene. Button-state meshes используют layout `0x0100` (`XYZ + Diffuse ARGB`, без UV), а текст представлен `spTextNode`, `spTextRenderable` и `spFont`.
- Чистый `Media/Menus/igmenu_opt_pc.smo` — другой GUI-вариант: 99 из 99 mesh
  строго декодируются и остаются плоскими после node-transform. Отдельные ветви
  `settings`, `controls`, `options`, `language`, `display` и `default` являются
  экранами/панелями, а `NORMAL*`, `HIGHLIGHTED*`, `PUSHED*`, `DISABLED*` и
  `shadow*` — слоями состояний. В нём есть 60 state-mesh и два mesh
  `GUICollision`, но нет text-классов; узлы `resolution`, `value_resolution` и
  `resolution_label` являются runtime-якорями. SHA-256 образца:
  `D6ED2606BFCA4C4EED41F59869D1EC1C20DFAFE7A3BFD8E7FEC6C5F109052F0E`.
- `Media/Menus/gameover.smo` — node-only 2D-layout размером 395 байт. Его шесть
  `spNode` образуют `gameover/{shadow,NORMAL}/text_*`; два leaf-слота наследуют
  transforms состояний. Mesh, texture, material, text-классов и самой строки в
  ресурсе нет: они должны создаваться runtime. SHA-256:
  `593DDE72EAFC36532B4976B5269EB53D0B3FAC217AB9B97472C6B8C1DBEBE2AA`.
- В partitioned `Alfea02.smo` object-directory intervals вкладывают `sector` и
  `portal` друг в друга, но это не transform-иерархия. `spStaticRenderObject`
  хранит готовую world-матрицу и завершает placement chain; baked mesh под
  sector group `0x94BBCA2A` сохраняет vertex coordinates. Объект `[4199]` —
  `spModel dormBigroomNOSH-000`, его mesh `[4201]` должен иметь identity world
  transform. SHA-256 образца:
  `1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF`.
- Наличие `Is32Bit()` в serializer подтверждает архитектурную поддержку 32-bit index buffers. Значение 65 535 нельзя считать доказанным общим лимитом Sparkplug.

## Связанные PC-анимации SAN/ANM

SAN использует тот же FFPS-контейнер, но хранит один animation object
`0x56EE563A`: duration и именованные position/rotation/scale curves. Curve
содержит служебный `UInt32=1`, `keyCount`, массив времени и затем Vector3 либо
quaternion XYZW. ANM является текстовой восьмиколоночной таблицей состояний со
ссылкой на SAN в последней колонке. На каталоге Bloom подтверждены 168/168 SAN;
подробный layout записан в
[`tools/SmoViewer/docs/SMO_FORMAT.md`](../../tools/SmoViewer/docs/SMO_FORMAT.md).

## Реализации

- [`SmoDocument`](../../tools/SmoViewer/SmoViewer.Core/SmoDocument.cs) — заголовок и каталог.
- [`SmoAnimationDecoder`](../../tools/SmoViewer/SmoViewer.Core/SmoAnimationDecoder.cs) — PC SAN curves.
- [`SmoClassRegistry`](../../tools/SmoViewer/SmoViewer.Core/SmoClassRegistry.cs) — известные class ID.
- [`SmoViewer.Inspect`](../../tools/SmoViewer/SmoViewer.Inspect/Program.cs) — человекочитаемый/JSON scan.
- [`SmoViewer.FormatTests`](../../tools/SmoViewer/SmoViewer.FormatTests/Program.cs) — synthetic и corpus checks.
- [`SMOTextureTool.Core`](../../tools/SMOTextureTool/SMOTextureTool.Core) — texture decode/repack.

Любое расширение спецификации сначала должно давать строгую диагностику на неизвестном варианте и только затем добавлять decode. Молчаливое угадывание offsets затрудняет проверку гипотез.
