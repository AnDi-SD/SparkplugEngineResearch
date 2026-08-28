# Формат SMO

Статус: структурная спецификация наблюдаемого корпуса. Все 1 149 SMO-копий и
36/36 встреченных классов строго разбираются; непроверенные runtime-семантики и
writer safety не считаются частью закрытого формата. Основной эталон — отдельный
`pc-pristine`/`ps2-pristine`, а `pc-working` используется только как изменяемый
сравнительный корпус. Техническая запись рядом с реализацией находится в
[`tools/SmoViewer/docs/SMO_FORMAT.md`](../../tools/SmoViewer/docs/SMO_FORMAT.md),
канонический backlog — в
[`../../research/open-questions.md`](../../research/open-questions.md), быстрые
игровые проверки — в
[`../research/smo-runtime-validation-plan.md`](../research/smo-runtime-validation-plan.md).

## Короткий ответ

SMO — это little-endian `FFPS`-контейнер сериализованного объектного графа Sparkplug, а не просто архив одного mesh. В одном файле могут быть модели, несколько мешей, материалы, текстуры, scene nodes, skin/collision и вспомогательные render-объекты. Каталог связывает имена и class ID с сериализованными областями данных; движок восстанавливает из них runtime-объекты.

Последнее предложение подтверждено формой каталога и набором зарегистрированных классов, но точный порядок создания объектов, владение и разрешение ссылок ещё требуют проверки по executable/runtime.

## Заголовок FFPS

Заголовок занимает `0x20` байт:

| Offset | Тип | Текущее значение/смысл |
|---:|---|---|
| `0x00` | `char[4]` | `FFPS` |
| `0x04` | `UInt32` | serializer version; игра требует `0x26` |
| `0x08` | `UInt32` | 15-битный export/session tag candidate; точный producer пока не установлен |
| `0x0C` | `UInt32` | объявленный размер файла |
| `0x10` | `UInt32` | platform mask: common `0x01`, PC `0x02`, PS2 `0x08` |
| `0x14` | `UInt32` | абсолютный `DataStart` |
| `0x18` | `UInt32` | `DataSize` |
| `0x1C` | `UInt32` | `ObjectCount` |

Для корректного исследованного файла ожидается:

```text
DataStart + DataSize == FileSize == actual file length
```

Общая checksum файла в подтверждённых полях не обнаружена. Игровые RGB-пробы
показали, что данные можно менять без обновления отдельной checksum, если
согласованы размеры/offsets и не нарушен object graph. Поле `0x08` не является
checksum: все 29 изменённых SMO в `pc-working` сохранили значение pristine,
корреляция с размером/offsets/числом объектов отсутствует, а обнулённое поле
успешно прошло native load как у `mousecursor.smo`, так и у контекстно
загруженного `bloom_jeans.smo`. Все 733 канонических PC/PS2-ресурса содержат
значение `0x0029..0x7FBE`; частые одинаковые значения объединяют серии разных
файлов. Поэтому `export/session tag candidate` — наиболее узкое рабочее описание, но
конкретный генератор поля ещё не доказан.

PC executable принимает заголовок, если `(platformMask & 0x03) != 0`; PS2 —
если `(platformMask & 0x09) != 0`. Следовательно, `1` — common, `2` — PC,
`8` — PS2, а `3` и `9` являются объединениями common с соответствующей
платформой. Проверка и runtime evidence записаны в
[`../research/smo-runtime-results.md`](../research/smo-runtime-results.md).

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

## Актуальная проверка PC-корпуса 2026-08-28

`SmoViewer.FormatTests` на `local-data/pc-pristine/Media` успешно обработал
416/416 SMO: 177 369 записей object directory, все 22 649 структурно валидных
`spMeshData`, 40 555 структурно валидных `spModel` и 22 149 mesh с доступной
строгой geometry-preview, 20 469 `spStaticRenderObject` и 745 620 известных
serializer fields (1 320 584 assertions). Разница mesh-счётчиков состоит из 494
native-only PS2 mesh и шести
представлений с NaN в optional UV/weights, а не из ошибок границ контейнера.

## Model и унаследованный renderable state

`spModel` содержит ровно две serializer-секции. Первая принадлежит
`spRenderable`:

1. optional field 0 — relationship к `spMaterialData`;
2. optional field 1 — relationship к `spFog`;
3. fields 2/3 — совместно присутствующие `UInt32 AlphaSortEnable` и `Priority`.

Финальная секция `spModel` содержит обязательный field 0 relationship к
`spMeshData` и optional field 1 `UInt32 ProjectionGroup`. Position, rotation и
scale в модели не сериализуются: transform задаётся окружающим node/static
object. Все присутствующие relationships строго проверяются по object ID,
inline size и target class ID.

Полный scan 118 720 объектов подтвердил семь вариантов присутствия полей,
включая два legacy PC-объекта с ID-only relationships. Viewer декодирует все
705 928 нетерминальных полей read-only. Частоты, PC/PS2-сопоставление и открытые
вопросы writer-а находятся в
[`../research/smo-class-sp-model.md`](../research/smo-class-sp-model.md).

## Static render object

`spStaticRenderObject` хранит одну serializer-секцию в порядке field `1`, field
`2`, field `0`, terminator. Fields 1/2 — affine row-vector `Matrix4x4`, field 0
— обязательный inline relationship к единственному `spModel`. Во всём корпусе
физический parent — `spPartitionNode`, но готовый world transform находится в
самом static object.

Field 2 следует engine convention, а не общему matrix inverse: верхний `3x3`
field 1 транспонируется, translation вычисляется как `-T*A^T`. Различие важно
при любом authored scale. Полный scan 61 851 объектов и межплатформенная
статистика описаны в
[`../research/smo-class-sp-static-render-object.md`](../research/smo-class-sp-static-render-object.md).

## Mesh data

Текущий строгий decoder подтверждает:

- cross-platform E0, Direct3D E1 и native PS2 field `1`;
- primitive type `2` как triangle list и `3` как triangle strip;
- позицию из трёх `Single` с offset `0` для известных layouts;
- для `0x093E` — четыре `Single` blend weights с offset `12` и четыре локальных
  `UInt8` palette indices с offset `28`;
- необходимость различать serialized stride и runtime vertex-buffer stride;
- PS2 header `40 + dmaQwordCount × 16` и 24-байтовый AABB;
- необходимость определять границы из структуры объекта, а не глобального
  поиска байтовой сигнатуры.

Известные примеры различия stride:

| Ресурс/layout | Runtime stride | Serialized stride |
|---|---:|---:|
| `fish.smo`, `0x093E` | 56 | 44 |
| `bloom_ball.smo`, `0x197E` | 76 | 64 |
| `loading.smo`, `0x0940` | 36 | 36 |

Полный корпусный разбор семи вариантов, 13 Direct3D layouts и native PS2
metadata находится в
[`../research/smo-class-sp-mesh-data.md`](../research/smo-class-sp-mesh-data.md).
Не закрыты команды DMA/VIF, optional attributes редкого `0x013E`, 32-bit index
storage и безопасная мутация.

## Материалы и текстуры

### Render state `spMaterialData`

Полный наблюдаемый PC/PS2 serializer-контракт, частоты всех 12 field ID,
legacy/current и single/multi-pass варианты, а также формы object relationship
зафиксированы в
[`../research/smo-class-sp-material-data.md`](../research/smo-class-sp-material-data.md).
Viewer декодирует этот контракт целиком, но writer остаётся отключён до
отдельной runtime-проверки мутаций.

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

Аудит всех трёх корпусов исправил старую классификацию PC texture layout.
`0x0EE3`/`0x32E3` — не format code: `E3` является field 3 с 32-битным размером,
а соседний байт принадлежит payload size. Подтверждённый `[656] top` имеет
Direct3D BGRA mip chain 256×256 с девятью уровнями. Viewer структурно читает
первый mip из вложенного field 0, последующие — из повторяемых fields 1; байт
`+0x3C` является частью `mipHeight`, а не отдельным marker.

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

Повторный структурный разбор уточнил и нативный trace 2026-08-14. BGRA payload
действительно начинается с `+0x3D` в наблюдаемой обёртке, но предыдущий байт —
старший байт `mipHeight`, а не самостоятельный marker. Старый writer начинал на
байт раньше и превращал height `0x00000100` в `0xFF000100`, поэтому игра читала
неверное число строк. Исправленная BGRA-запись пережила нативное наблюдение; это
подтверждает границу descriptor/pixels и загрузку Alpha, но не прежнюю гипотезу
об отдельном marker и не визуальную семантику конкретного blend state.

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

## Результат полного `spMeshData` scan

Research schema v2 и строгий decoder заменили прежний baseline частично
модифицированного каталога. Полностью прочитаны 66 191 уникальный `spMeshData`:
22 649 в каждом PC-корпусе и 20 893 на PS2. Все 87 330 непустых прямых полей
укладываются в семь cross-platform/Direct3D/native-PS2 вариантов; triangle list
`primitiveType = 2` подтверждён и поддерживается.

Прежние «494 PS2 preamble» и «137 PS2 boundary» в PC не были повреждениями.
Это 629 настоящих native PS2 mesh в пяти `Menus/*_ps2.smo`; 135 из них также
хранят cross-platform copy. PC executable регистрирует PS2 mesh serializer,
поэтому тип field `1` теперь определяется внутренним layout, а не платформой
корпуса. Полный разбор, таблицы форматов и границы PS2 DMA/VIF:
[`smo-class-sp-mesh-data.md`](../research/smo-class-sp-mesh-data.md).

## Уточнения корпуса 2026-08-10

- `spNode`-иерархия skeleton восстанавливается по `esfNodeChild` (field type `5`), а не по каталожному `ParentIndex`, который описывает владение сериализованными интервалами. Поддерживаются обычная форма `objectId + inlineSize + optional SBOO` и найденная в двух PC-ресурсах компактная форма из одного `objectId`.
- `spRenderNode` использует две законченные serializer-секции: полный
  унаследованный контракт `spNode` и собственный повторяемый field 0
  `esfRenderNodeRenderable`. Renderable relationship ведёт на `spModel`,
  `spSkin`, `spParticleSystem` или `spLensFlare` и допускает ID-only,
  sized-reference и inline-object encodings. Все 38 443 объекта трёх корпусов
  подтверждают один PC/PS2 layout; подробности в
  [`smo-class-sp-render-node.md`](../research/smo-class-sp-render-node.md).
- PC-layouts `0x097E` и `0x197E` хранят четыре `float32`-веса по `+12` и четыре байтовых индекса локальной bone palette по `+28`; `spSkin` отдельно хранит palette и inverse-bind matrices.
- В исследованном PC-корпусе palette имеет 16 slots, в PS2-корпусе — 64. Независимый Kikko rig использует для тела PC palettes `16 + 9`, тогда как PS2 хранит те же 20 уникальных костей в одной palette. Автоматическое разбиение exporter'ом остаётся сильной гипотезой.
- Полный scan подтвердил три serializer-секции `spSkin`: inherited
  `spRenderable`, inherited `spModel` и собственный `esfSkin`. Его payload
  начинается с blend-influence hint и slot count, затем повторяет
  `spNode relationship + inverse-bind Matrix4x4`. Все 40 704 матрицы трёх
  корпусов affine и invertible; подробности в
  [`smo-class-sp-skin.md`](../research/smo-class-sp-skin.md).
- Первое слово `esfSkin` не является reserved: PC хранит 0..4. Для всех 66
  декодируемых копий с ненулевым значением 1..4 оно совпадает с максимальным
  числом активных blend weights на вершину; ноль остаётся default/unspecified.
- `spMeshBV` имеет общий PC/PS2 version-2 layout: обязательный field 0 хранит
  triangle count, `UInt16` indices, vertex count и `Vector3` positions.
  Необязательный field 1 — массив `wxFaceData` (`0x313C4C17`) с разреженными
  `UInt8 surface type`, `UInt16 flags` и `UInt8 surface ID`, ровно по одной
  записи на треугольник. Surface type 1..9 восстановлены как stone, dirt,
  grass, water, snow, swamp, mud, deepwater и carpet. Подробности:
  [`smo-class-sp-mesh-bv.md`](../research/smo-class-sp-mesh-bv.md).
- `spPartitionRenderable` имеет одну собственную serializer-секцию: обязательный
  field 1 хранит `UInt32` ARGB `DebugColor`, затем 1..67 field 0 содержат inline
  physical-child relationships на `spModel`, после чего идёт пустой field 0
  terminator. Все 19 989 отношений трёх корпусов имеют одну encoding-форму;
  подробности в
  [`smo-class-sp-partition-renderable.md`](../research/smo-class-sp-partition-renderable.md).
- `spPartitionNode` использует fields `0..7`: обязательные `DebugColor`,
  `PartitionSystem`, `Zone` и `PartitionRenderable`, а между ними — повторяемые
  `Child`, `CollisionInfo`, `ZonePortal` и `StaticRenderObject`. Field 2 `Child`
  отсутствует у точного класса, но встречается 18 048 раз в унаследованных
  секциях `spOctreeNode`: `UInt32 slot` плюс inline relationship. Остальные
  232 035 отношений всех трёх корпусов разрешаются на ожидаемые классы;
  подробности в
  [`smo-class-sp-partition-node.md`](../research/smo-class-sp-partition-node.md).
- `spOctreeNode` содержит базовую partition-секцию с ровно восемью field 2
  slots 0..7, затем собственные `Vector3 Pivot`, `Mins`, `Maxs`. Bits 0/1/2
  slot выбирают high-половину X/Y/Z; bounds всех вложенных octree-children
  точно совпадают с выбранным октантом. Все 2 256 PC/PS2-объектов строго
  декодированы; подробности в
  [`smo-class-sp-octree-node.md`](../research/smo-class-sp-octree-node.md).
- `spBSPNode` также наследует partition-секцию, но хранит ровно два field 2 со
  slots 0/1. Каждый child является inline `spBSPNode` либо sized reference на
  terminal `spPartitionNode`; собственный обязательный field 0 хранит
  нормализованную Plane (`Vector3 normal + Single constant`). Optional field 1
  Polygon поддержан PC/PS2 serializers, но отсутствует во всех 324 объектах.
  54 дерева удовлетворяют формуле `SerializedSize = 101 * split count`;
  подробности в
  [`smo-class-sp-bsp-node.md`](../research/smo-class-sp-bsp-node.md).
- `spOcclusionVolume` наследует `spNode` и добавляет обязательные field 0
  IndexBuffer (`2, triangleCount, 0, UInt16[3*T]`) и field 1 VertexBuffer
  (`0, vertexCount, 0, Vector3[V]`). Все 60 PC/PS2-объектов строго декодируются
  и используют общий побайтно совпадающий layout. Корпус содержит только
  connected planar disks с `T=V-2`; 54/60 строго выпуклые, а шесть копий одного
  decagon имеют неглубокую вогнутость. Подробности:
  [`smo-class-sp-occlusion-volume.md`](../research/smo-class-sp-occlusion-volume.md).
- `spMeshNavigationSet` имеет три секции: `spNode`, `spNavigationSet` и
  собственную. Base-секция хранит `UInt32 NodeCount`, матрицы `N x N` и `P x N`,
  таблицу `UInt8 nodeId + UInt32 count + UInt8 neighbours[]`, repeated portal
  relationships и Boolean `Enabled`; собственный field 0 ссылается на
  `spMeshBV`. Matrix cell выбирает ordered outgoing link. Значение 3 является
  terminal/unreachable marker, когда выходит за degree текущего узла, но при
  degree 4 остаётся обычным selector. Полный разбор:
  [`smo-class-sp-mesh-navigation-set.md`](../research/smo-class-sp-mesh-navigation-set.md).
- `spPartitionSystem` наследует `spRenderNode` и имеет три секции. В node-секции
  записаны `IsStatic`, `IsAnimated`, zones/portal nodes и collision references;
  унаследованная renderable-секция в корпусе пуста. Собственный обязательный
  field 0 `PartitionRoot` является inline `spBSPNode` либо sized reference на
  `spOctreeNode`. Все 88 PC/PS2-объектов строго декодированы; подробности в
  [`smo-class-sp-partition-system.md`](../research/smo-class-sp-partition-system.md).
- `spZone` наследует `spNode` и имеет две секции. Node-секция обязательно хранит
  Position и `Animated=false`, иногда Rotation и `Static=true`; собственная
  секция повторяет field 0 `LocalPartitionRoot` 0..4 раза. Каждый root — owned
  inline `spPartitionNode` либо его `spOctreeNode`-подкласс. Полный разбор:
  [`smo-class-sp-zone.md`](../research/smo-class-sp-zone.md).
- `spZonePortal` имеет одну секцию с обязательными field 0 `DestinationZone`,
  field 1 `Polygon` (`UInt32 count + Vector3[count]`) и field 2 `Open`
  (`Boolean`). Source zone берётся из field 3 физического родителя
  `spPartitionNode`; destination бывает inline-owned либо reference. Во всех
  трёх корпусах 618 portals открыты и имеют четыре вершины. 103 пары на корпус
  задают обратные рёбра и используют polygon с точно обратным winding. Полный
  разбор: [`smo-class-sp-zone-portal.md`](../research/smo-class-sp-zone-portal.md).
- `spZonePortalNode` наследует `spNode`, поэтому сначала хранит обязательные
  Position и `Animated=false` с optional Rotation/Static. Собственная вторая
  секция содержит ровно два повторяемых field 0 `ZonePortal` в sized-reference
  форме: сначала `BackToFront`, затем `FrontToBack`. Узел не дублирует polygon;
  Position совпадает с его центроидом лишь у 69 из 103 объектов на корпус.
  Полный разбор:
  [`smo-class-sp-zone-portal-node.md`](../research/smo-class-sp-zone-portal-node.md).
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

SAN использует тот же FFPS-контейнер, но хранит один `spAnimation`
(`0x56EE563A`): duration и именованные position/rotation/scale curves. Curve
содержит служебный `UInt32=1`, `keyCount`, массив времени и затем Vector3 либо
quaternion XYZW. ANM является текстовой восьмиколоночной таблицей состояний со
ссылкой на SAN в последней колонке. На каталоге Bloom подтверждены 168/168 SAN;
подробный layout записан в
[`tools/SmoViewer/docs/SMO_FORMAT.md`](../../tools/SmoViewer/docs/SMO_FORMAT.md).
Связь имён EXE/ANM/SAN/SMO и статистика Алфеи записаны в
[отдельном исследовании](../research/alfea-unknown-resources.md).

Нативная PC-проверка same-length mutation `R_Ankle -> r_Ankle` подтверждает
точное регистрозависимое сопоставление. Контейнер с изменённым именем загружается,
но новый ключ проходит отдельную ветвь `char_traits<char>::compare`-дерева и не
получает ни exact, ни cross-case равенства; pristine-модель получает точное
`R_Ankle == R_Ankle`. Поэтому имя track/node является строковым ключом, а не
pointer или регистронезависимым alias. Case-only совпадение следует показывать
как диагностическую ошибку привязки.

Отсутствующий exact target не делает SMO невалидным: one-byte parent и leaf
renames прошли PC contextual load, вернули ненулевой resource и не вызвали crash.
Duplicate names в обоих порядках также загружаются, но сворачиваются в один
строковый key, а track противоположного имени становится missing. Визуальный
bind-pose/descendant fallback и first/last/all-target выбор требуют отдельного
transform/frame probe и пока не являются частью спецификации.

## Реализации

- [`SmoDocument`](../../tools/SmoViewer/SmoViewer.Core/SmoDocument.cs) — заголовок и каталог.
- [`SmoAnimationDecoder`](../../tools/SmoViewer/SmoViewer.Core/SmoAnimationDecoder.cs) — PC SAN curves.
- [`SmoClassRegistry`](../../tools/SmoViewer/SmoViewer.Core/SmoClassRegistry.cs) — известные class ID.
- [`SmoViewer.Inspect`](../../tools/SmoViewer/SmoViewer.Inspect/Program.cs) — человекочитаемый/JSON scan.
- [`SmoViewer.FormatTests`](../../tools/SmoViewer/SmoViewer.FormatTests/Program.cs) — synthetic и corpus checks.
- [`SMOTextureTool.Core`](../../tools/SMOTextureTool/SMOTextureTool.Core) — texture decode/repack.

Любое расширение спецификации сначала должно давать строгую диагностику на неизвестном варианте и только затем добавлять decode. Молчаливое угадывание offsets затрудняет проверку гипотез.
