# `spMeshData` (`0x33C34CF0`)

Статус: **полный структурный read-only decode всех наблюдаемых PC/PS2-контейнеров
и PC vertex/index layouts**. Native PS2 DMA/VIF stream пока ограниченно раскрыт:
его заголовок, размер, флаги каналов, bounding sphere и отдельный AABB известны,
но команды VIF и упакованные vertex streams ещё не декодируются.

## Охват

| Корпус | Уникальные SMO | Уникальные `spMeshData` |
|---|---:|---:|
| `pc-working` | 396 | 22 649 |
| `pc-pristine` | 396 | 22 649 |
| `ps2-pristine` | 303 | 20 893 |

Строгий контейнерный decoder перечитал все **66 191** уникальных mesh-объектов и
**87 330** непустых прямых полей до точной границы объекта. PC working и pristine
совпадают по полному нормализованному содержимому mesh во всех 396 ресурсах.

## Контракт полей

| ID | Semantic key | Содержимое | Наблюдений |
|---:|---|---|---:|
| 0 | `mesh.cross_platform` | portable primitive/index/vertex buffers | 1 865 |
| 1 | `mesh.platform_specific` | Direct3D E1 либо native PS2 header + DMA/VIF | 64 605 |
| 2 | `mesh.bounding_box` | `Vector3 min` + `Vector3 max`, 24 байта | 20 860 |

После непустых полей всегда находится нулевой field `0` — terminator объекта.
Порядок строгий: optional field `0`, optional field `1`, optional field `2`,
terminator. Хотя номера полей одинаковы на обеих платформах, семантика field `1`
определяется его внутренним layout, а не происхождением SMO.

### Семь фактических вариантов

| Вариант | `pc-working` | `pc-pristine` | `ps2-pristine` |
|---|---:|---:|---:|
| только cross-platform | 793 | 793 | 0 |
| только Direct3D | 21 225 | 21 225 | 0 |
| cross-platform + Direct3D | 2 | 2 | 0 |
| только native PS2, без AABB | 494 | 494 | 33 |
| cross-platform + native PS2, без AABB | 135 | 135 | 0 |
| native PS2 + AABB | 0 | 0 | 20 855 |
| cross-platform + native PS2 + AABB | 0 | 0 | 5 |

Это семь **representation-container variants**, а не семь форматов вершин.

## Native PS2-меши внутри PC-корпуса

Важное исключение из прежней модели: PC-корпус содержит **629 native PS2 mesh**
в каждом из working/pristine наборов. Все они находятся ровно в пяти ресурсах:

| Ресурс | Native PS2 mesh |
|---|---:|
| `Menus/mmenu_cont_ps2.smo` | 247 |
| `Menus/mmenu_new_ps2.smo` | 247 |
| `Menus/igmenu_opt_ps2.smo` | 54 |
| `Menus/igmenu_diary_ps2.smo` | 43 |
| `Menus/igmenu_fash_ps2.smo` | 38 |

Следовательно, `platform=PC` в индексе говорит о расположении файла, но не
гарантирует Direct3D layout каждого field `1`. Эти ресурсы могут быть packaged
остатками или использоваться отдельным путём; факт их runtime-загрузки PC-игрой
пока не доказан. Наличие `spPS2MeshData` и его serializer в PC executable
подтверждает, что поддержка класса в PC-сборке существует.

## Cross-platform E0

```text
UInt32 primitiveType        2 = triangle list, 3 = triangle strip
UInt32 storedCount
UInt32 reserved             0
UInt16 indices[...]         число зависит от primitiveType
[UInt32 extra]              два последних strip-index либо CDCDCDCD padding
UInt32 vertexFormat
UInt32 vertexCount
UInt32 reserved             0
Byte   vertices[...]        vertexCount × serializedStride
```

Для triangle list `storedCount` является числом треугольников, поэтому индексный
буфер содержит `storedCount × 3` значений. Для strip встречаются две формы:
заявлен полный count либо два последних `UInt16` лежат в дополнительном слове.
Слово `0xCDCDCDCD` является padding, а не индексами. Decoder различает формы по
единственной структурно допустимой позиции vertex header и точной границе payload.

В одном PC-корпусе field `0` встречается 930 раз; PS2 добавляет пять экземпляров
формата `0x0900`. PC-распределение: `0000:18`, `0040:14`, `0100:87`, `013E:2`,
`0140:13`, `0800:21`, `0840:20`, `0900:427`, `093E:13`, `0940:271`,
`097E:25`, `1940:19`.

## Direct3D E1

```text
UInt32 vertexFormat
UInt32 vertexCount
UInt32 runtimeVertexBytes
UInt32 serializedIndexBytes
Byte   fieldSeparator       0

UInt32 primitiveType        2 = triangle list, 3 = triangle strip
UInt32 primitiveCount
UInt32 reserved             0
UInt16 indices[...]

UInt32 vertexFormat         повтор первого значения
UInt32 vertexCount          повтор первого значения
UInt32 reserved             0
Byte   vertices[...]        vertexCount × serializedStride
```

Для triangle list `serializedIndexBytes = primitiveCount × 3 × 2`, для strip —
`(primitiveCount + 2) × 2`. Все индексы текущего корпуса 16-битные. В executable
есть `Is32Bit()`, поэтому отсутствие 32-битных индексов в Winx-корпусе нельзя
считать ограничением всего Sparkplug.

Из 21 227 Direct3D-представлений одного PC-корпуса 14 являются triangle list
(по семь `0x0940` и `0x097E`), остальные 21 213 — triangle strip.

### Наблюдаемые Direct3D vertex layouts

| Format | Mesh | Disk stride | Runtime stride | Подтверждённые атрибуты после XYZ |
|---:|---:|---:|---:|---|
| `0x0000` | 2 | 12 | 12 | — |
| `0x0040` | 3 | 24 | 24 | normal |
| `0x0100` | 70 | 16 | 16 | diffuse ARGB |
| `0x0140` | 26 | 28 | 28 | normal, diffuse ARGB |
| `0x0800` | 73 | 20 | 20 | UV0 |
| `0x0840` | 477 | 32 | 32 | normal, UV0 |
| `0x0900` | 6 504 | 24 | 24 | diffuse ARGB, UV0 |
| `0x093E` | 30 | 44 | 56 | 4 weights, 4 palette indices, diffuse, UV0 |
| `0x0940` | 12 090 | 36 | 36 | normal, diffuse, UV0 |
| `0x097E` | 587 | 56 | 68 | weights, indices, normal, diffuse, UV0 |
| `0x1900` | 964 | 32 | 32 | diffuse, UV0, UV1 |
| `0x1940` | 310 | 44 | 44 | normal, diffuse, UV0, UV1 |
| `0x197E` | 91 | 64 | 76 | weights, indices, normal, diffuse, UV0, UV1 |

Первые 12 байт всегда являются `XYZ`. Skinning-layouts хранят четыре `Single`
weights с offset `12` и четыре локальных palette index с offset `28`. Разница
disk/runtime stride у `*3E`/`*7E` соответствует runtime-нормали, которой нет в
сериализованном stream. Редкий cross-only `0x013E` имеет stride 36; его XYZ и
структура буфера известны, но optional attributes пока намеренно не названы.

Структурный decode отделён от строгого preview атрибутов. В каждом PC-корпусе
шесть представлений имеют валидные размеры, индексы и XYZ, но содержат NaN в
optional UV/weights: четыре mesh в `Alfea_broken_01/03.smo` и два в
`SFX/Goopmonster.smo`. Поэтому контейнеры занесены в БД как валидные, а renderer
по-прежнему не принимает нечисловые атрибуты. Строгий preview доступен для
44 298 из 45 298 PC-объектов двух корпусов; ещё 988 — native-only PS2 meshes, а
12 — перечисленные intentionally broken representations.

## Native PS2 field 1

Для всех 20 893 PS2-корпусных mesh и 1 258 копий в двух PC-корпусах выполняется:

```text
Vector4 boundingSphere      center XYZ + radius
UInt32  primitiveCount
UInt32  vertexCount
UInt32  vertexFormat
UInt32  dmaQwordCount
UInt32  additionalUvCount   0 или 1
UInt32  blendWeightCount    0 или 4
Byte    dmaVif[dmaQwordCount × 16]
```

То есть точный payload size равен `40 + dmaQwordCount × 16`. Sphere конечна,
radius неотрицателен. В PS2 pristine наблюдаются форматы:

| Format | Mesh |
|---:|---:|
| `0x0840` | 58 |
| `0x0900` | 18 269 |
| `0x093E` | 21 |
| `0x0940` | 706 |
| `0x097E` | 210 |
| `0x1900` | 1 537 |
| `0x1940` | 61 |
| `0x197E` | 31 |

`additionalUvCount = 1` ровно у всех 1 629 форматов с битом `0x1000`.
`blendWeightCount = 4` ровно у всех 262 форматов `0x093E/097E/197E`.
Комбинации флагов: `0/0: 19 033`, `0/4: 231`, `1/0: 1 598`, `1/4: 31`.
Это подтверждает назначение двух последних слов независимо от PC layout.

Внутренняя раскладка DMA/VIF qwords ещё не считается раскрытой: безопасный writer
и геометрический preview для native PS2 не реализованы.

## Field 2: bounding box

Все 20 860 field `2` состоят ровно из шести конечных `Single`:

```text
minX, minY, minZ, maxX, maxY, maxZ
```

Для каждой оси `min <= max`. У 33 PS2 mesh field `2` отсутствует; это допустимый
вариант, сосредоточенный в `Menus/SideQuestObject` и нескольких SFX. Нельзя
требовать, чтобы bounding sphere содержала все восемь углов AABB: sphere строится
по геометрии, а углы осевого box обычно не принадлежат mesh. Такое включение
случайно выполняется лишь для 8 979 из 20 860 пар.

## Сопоставление PC и PS2

По каноническому пути найдено 303 общих mesh-ресурса. В 202 совпадает число mesh,
что даёт 17 761 надёжную ordinal-пару. Для всех пар доступны PC и PS2 headers:

| Проверка | Совпадений |
|---|---:|
| vertex format | 6 614 / 17 761 |
| primitive count | 17 107 / 17 761 |
| vertex count | 3 268 / 17 761 |

Высокое совпадение primitive count и низкое совпадение vertex count показывают,
что PS2-представление часто пересобирает вершины под DMA/VIF, сохраняя топологию.
Это вывод из корреляции, а не доказательство точного значения каждого PS2 word.

## Свидетельства executable

Оба executable содержат имена:

- `spMesh`, `spMeshData`, `spMeshDataSerializer`, `spPlatformSpecificMeshData`;
- `spPS2MeshData`, `spPS2MeshDataSerializer`;
- `esfMeshDataCrossPatform` — именно с опечаткой `Patform`;
- `esfMeshDataPlatformSpecific`, `esfMeshDataBoundingBox`.

PC executable дополнительно содержит `spDXMeshData` и
`spDXMeshDataSerializer`. Совпадение имён с тремя полями и независимая структура
корпуса подтверждают семантику контейнера.

## Реализация и воспроизведение

`SmoMeshDataDecoder` строго разбирает контейнер, оба PC layouts, PS2 header и AABB.
`SmoMeshDecoder` теперь выбирает field `1` Direct3D, но при mixed/PS2 layout умеет
перейти к field `0`; Viewer показывает структурные значения всех трёх полей даже
тогда, когда native PS2 geometry ещё нельзя отрисовать. Registry содержит 82
подтверждённых definitions, из них три относятся к `spMeshData`.

```powershell
python research\analyze_smo_mesh_data.py `
  local-data\results\smo-corpus-v2.sqlite

dotnet run --project tools\SmoViewer\SmoViewer.Inspect -- `
  research-db analyze-class local-data\results\smo-corpus-v2.sqlite `
  spMeshData --json
```

Analyzer перечитывает исходные directory/PCK bytes, назначает вариант всем 66 191
объектам, аннотирует 87 330 полей, сверяет оба executable и идемпотентно обновляет
field definitions, variants, assignments и evidence.

## Открытые вопросы

1. Декодировать DMA/VIF stream PS2 до позиций, индексов и остальных vertex channels.
2. Подтвердить дизассемблированием точную роль обоих PS2 count words и правила
   построения bounding sphere/AABB.
3. Раскрыть optional attributes редкого cross-platform `0x013E`.
4. Найти настоящий 32-bit index-buffer sample и восстановить его storage layout.
5. Проверить fixed-size и затем structural mutation непосредственно в PC/PS2 игре;
   до этого writer для `spMeshData` остаётся небезопасным.

Пункты 1–4 отложены до разработки platform/writer частей. Ближайший runtime-этап
использует только существующие geometry layouts и не пытается перепаковывать
PS2 DMA/VIF: [`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
