# spMeshData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMeshData](../../../Sparkplug/Code/Sparkplug/spMeshData.h).

### Идентичность

`spMeshData` имеет class ID `0x33C34CF0` и direct base `spMesh`
(`0x3F077B6C`) на обеих платформах.

| Факт | PC | PS2 |
| --- | ---: | ---: |
| initializer | `0x006D1D00` | `0x00480E2C` |
| RTTI clone | `0x0041AC00` | `0x00134CF0` |
| init/deep-copy inputs | `0x00434E40` | `0x0015D220` |
| deep-copy state | `0x00434F10` | `0x0015D1A0` |
| release buffers | destructor path | `0x0015D120` |
| размер | observed exact extent `0x58` | factory allocation `0x58` |

### Layout и ownership

`spMeshData` добавляет к `spMesh` ровно два owning pointers:

| Offset | Тип/роль |
| ---: | --- |
| `+0x50` | `spIndexBuffer*` |
| `+0x54` | `spVertexBuffer*` |

Одинаковые offsets независимо видны в PC `0x00434E40/0x00434F10` и PS2
`0x0015D120/0x0015D1A0/0x0015D220`. PC вызывает deep-copy helpers
`0x0045FD90` и `0x00460240`; PS2 — уже сопоставленный deep-copy
`spIndexBuffer` `0x00159780` и vertex helper `0x0015CB30`.

Portable constructor намеренно ставит оба owner в `nullptr`. Это явно
документированная safety-divergence: воспроизводить неопределённые pointers и
риск destructor-а нельзя, а доказанные layout и последующая init-семантика от
этого не меняются.

RTTI clone не равен этому deep-copy: обе vtable оставляют в copy slot
унаследованный name-copy (`0x00413120` PC / `0x00105DC0` PS2). Clone wrappers
`0x0041AC00/0x00134CF0` создают объект и вызывают именно этот virtual slot.
Следовательно, как у `spIndexBuffer`, полноценное копирование геометрии живёт
в отдельном API (`0x00434F10/0x0015D1A0`), а RTTI clone является blank runtime
clone. Это различие должно сохраниться в будущей реализации.

Открыты original header/TU/API, роль secondary vtable/interface, причины
неинициализированных native owners, trailing поля `spMesh`, platform-specific
mesh subclasses и точные failure/rollback semantics. SMO container grammar
остаётся в отдельной карточке [`smo-class-sp-mesh-data.md`](sp-mesh-data.md)
и не подменяет native object API.

## Данные в SMO

Статус: **полный структурный read-only decode всех наблюдаемых PC/PS2-контейнеров
и PC vertex/index layouts**. Native PS2 DMA/VIF stream пока ограниченно раскрыт:
его заголовок, размер, флаги каналов, bounding sphere и отдельный AABB известны,
но команды VIF и упакованные vertex streams ещё не декодируются.

### Охват

Строгий контейнерный decoder перечитал все **66 191** уникальных mesh-объектов и **87 330** непустых прямых полей до точной границы объекта.

### Контракт полей

| ID | Semantic key | Содержимое | Наблюдений |
| ---: | --- | --- | ---: |
| 0 | `mesh.cross_platform` | portable primitive/index/vertex buffers | 1 865 |
| 1 | `mesh.platform_specific` | Direct3D E1 либо native PS2 header + DMA/VIF | 64 605 |
| 2 | `mesh.bounding_box` | `Vector3 min` + `Vector3 max`, 24 байта | 20 860 |

После непустых полей всегда находится нулевой field `0` — terminator объекта.
Порядок строгий: optional field `0`, optional field `1`, optional field `2`,
terminator. Хотя номера полей одинаковы на обеих платформах, семантика field `1`
определяется его внутренним layout, а не происхождением SMO.

#### Семь фактических вариантов

| Вариант | `pc-working` | `pc-pristine` | `ps2-pristine` |
| --- | ---: | ---: | ---: |
| только cross-platform | 793 | 793 | 0 |
| только Direct3D | 21 225 | 21 225 | 0 |
| cross-platform + Direct3D | 2 | 2 | 0 |
| только native PS2, без AABB | 494 | 494 | 33 |
| cross-platform + native PS2, без AABB | 135 | 135 | 0 |
| native PS2 + AABB | 0 | 0 | 20 855 |
| cross-platform + native PS2 + AABB | 0 | 0 | 5 |

Это семь **representation-container variants**, а не семь форматов вершин.

### Cross-platform E0

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

### Direct3D E1

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

#### Наблюдаемые Direct3D vertex layouts

| Format | Mesh | Disk stride | Runtime stride | Подтверждённые атрибуты после XYZ |
| ---: | ---: | ---: | ---: | --- |
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

### Native PS2 field 1

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

То есть точный payload size равен `40 + dmaQwordCount × 16`. Sphere конечна, radius неотрицателен.

| Format | Mesh |
| ---: | ---: |
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

### Field 2: bounding box

Все 20 860 field `2` состоят ровно из шести конечных `Single`:

```text
minX, minY, minZ, maxX, maxY, maxZ
```

Для каждой оси `min <= max`. У 33 PS2 mesh field `2` отсутствует; это допустимый
вариант, сосредоточенный в `Menus/SideQuestObject` и нескольких SFX. Нельзя
требовать, чтобы bounding sphere содержала все восемь углов AABB: sphere строится
по геометрии, а углы осевого box обычно не принадлежат mesh. Такое включение
случайно выполняется лишь для 8 979 из 20 860 пар.

### Сопоставление PC и PS2

По каноническому пути найдено 303 общих mesh-ресурса. В 202 совпадает число mesh,
что даёт 17 761 надёжную ordinal-пару. Для всех пар доступны PC и PS2 headers:

Высокое совпадение primitive count и низкое совпадение vertex count показывают,
что PS2-представление часто пересобирает вершины под DMA/VIF, сохраняя топологию.
Это вывод из корреляции, а не доказательство точного значения каждого PS2 word.

### Открытые вопросы

1. Декодировать DMA/VIF stream PS2 до позиций, индексов и остальных vertex channels.
2. Подтвердить дизассемблированием точную роль обоих PS2 count words и правила
   построения bounding sphere/AABB.
3. Раскрыть optional attributes редкого cross-platform `0x013E`.
4. Найти настоящий 32-bit index-buffer sample и восстановить его storage layout.
5. Проверить fixed-size и затем structural mutation непосредственно в PC/PS2 игре;
   до этого writer для `spMeshData` остаётся небезопасным.
