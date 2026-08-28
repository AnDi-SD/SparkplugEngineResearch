# `spMeshBV` (`0x3F453DE7`)

Статус: полностью восстановлен наблюдаемый PC/PS2 layout индексированной
collision-геометрии и необязательного массива `wxFaceData`. Все экземпляры
трёх корпусов строго декодируются до конца. Значения доступны в read-only
Inspector и записаны в schema v2; изменение face metadata пока не разрешено,
поскольку runtime mutation не проверена.

## Распространённость

| Корпус | Объекты | Уникальные SMO | Физические вхождения | Geometry only | С face data |
|---|---:|---:|---:|---:|---:|
| `pc-working` | 3 609 | 95 | 3 609 | 2 400 | 1 209 |
| `pc-pristine` | 3 609 | 95 | 3 609 | 2 400 | 1 209 |
| `ps2-pristine` | 3 295 | 69 | 5 371 | 2 110 | 1 185 |

Всего в индексе 10 513 уникальных объектов и 259 пар
`corpus/resource`. У всех отсутствует имя. PS2 physical count больше unique
count из-за повторного включения одних и тех же SMO в несколько PCK.

Физические родители подтверждают две роли класса:

| Родитель | Объекты трёх корпусов |
|---|---:|
| `spCollisionInfo` | 10 158 |
| `spMeshNavigationSet` | 351 |
| корень SMO | 4 |

То есть один и тот же mesh-BV используется как обычный collision primitive и
как треугольная основа навигации. Родитель определяет назначение, но не меняет
собственный layout `spMeshBV`.

## Верхний serializer

Собственная секция имеет две наблюдаемые формы:

```text
field 0: esfMeshBV                 required geometry
field 0, empty: section terminator

field 0: esfMeshBV                 required geometry
field 1: esfMeshBVFaceData         optional wxFaceData array
field 0, empty: section terminator
```

Это два варианта присутствия, а не разные платформенные форматы. Обе формы
массово встречаются на PC и PS2. Полный serialized size меняется вместе с числом
треугольников, вершин и разреженных face-полей; он не является discriminator
подтипа.

## Field 0: индексированная геометрия

Payload `esfMeshBV` одинаков на PC и PS2 и целиком little-endian:

| Порядок | Тип | Смысл |
|---:|---|---|
| 1 | `UInt32` | версия, во всех объектах `2` |
| 2 | `UInt32` | число треугольников `T` |
| 3 | `UInt32` | ноль |
| 4 | `UInt16[T*3]` | triangle-list indices |
| 5 | `UInt32` | ноль после индексов |
| 6 | `UInt32` | число вершин `V` |
| 7 | `UInt32` | ноль |
| 8 | `Vector3[V]` | позиции `Single X/Y/Z` |

Формула размера payload:

```text
24 + T * 6 + V * 12 bytes
```

Все индексы находятся внутри массива вершин, все координаты конечны, а payload
заканчивается точно после последней позиции. В доступном корпусе `T=1..1064`,
`V=3..754`. Primitive является triangle list: каждые три последовательных
индекса образуют грань.

| Корпус | Треугольники | Вершины | Вырожденные треугольники |
|---|---:|---:|---:|
| `pc-pristine` | 98 927 | 83 440 | 21 |
| `ps2-pristine` | 93 211 | 78 674 | 21 |

63 вырожденных треугольника в сумме трёх профилей включают одну и ту же PC
геометрию дважды. Decoder принимает их как реальные сериализованные данные, а
Inspector только показывает диагностический счётчик.

## Field 1: массив `wxFaceData`

Необязательный блок начинается с собственного class ID и содержит ровно одну
запись на треугольник:

```text
UInt32 0x313C4C17       // wxFaceData class ID
UInt32 faceCount        // всегда равно T
repeat faceCount times:
    optional field 1, size 1: UInt8  m_uSurfaceType
    optional field 2, size 2: UInt16 m_uFlags
    optional field 3, size 1: UInt8  m_uSurfaceID
    field 0                        // terminator текущей грани
```

ID и размер вложенных членов записаны serializer small-int: base-128 unsigned
целое с continuation bit. Порядок наблюдаемых членов возрастающий. Нулевые
значения являются default и опускаются независимо, поэтому одна запись может
состоять только из terminator. `SerializedFieldMask` в decoder сохраняет разницу
между «член отсутствовал и дал default 0» и «в файле явно записан ненулевой
член»; нулевые payload writer не создаёт.

Полный scan дал 106 832 face records в трёх профилях. Непустые члены встречаются:

| Член | Вхождения |
|---|---:|
| `m_uSurfaceType` | 84 326 |
| `m_uFlags` | 23 452 |
| `m_uSurfaceID` | 8 591 |

## Тип поверхности

PC converter и совпадающий набор PS2-строк подтверждают значения:

| Значение | Имя executable | PC pristine | PS2 pristine |
|---:|---|---:|---:|
| 0 | fallback/unspecified | 7 778 | 6 950 |
| 1 | `stone` | 16 790 | 16 592 |
| 2 | `dirt` | 1 276 | 1 276 |
| 3 | `grass` | 4 191 | 4 133 |
| 4 | `water` | 648 | 648 |
| 5 | `snow` | 4 448 | 4 376 |
| 6 | `swamp` | 0 | 0 |
| 7 | `mud` | 167 | 167 |
| 8 | `deepwater` | 12 | 12 |
| 9 | `carpet` | 686 | 686 |

Значение 6 поддержано runtime converter, хотя в доступных SMO не встретилось.
Причина выбора материала для каждой поверхности — вероятнее всего внешний
gameplay/audio consumer, но такая роль ещё не доказана и в базе не утверждается.

## Flags и surface ID

`m_uFlags` реально является 16-битным полем, а `m_uSurfaceID` — отдельным
8-битным полем. Это подтверждено кодом serializer обеих платформ и разреженным
байтовым потоком. Flags использует множество одиночных битов и комбинаций,
включая частые `0`, `1920`, `512`, `256`, `16384`, `8192` и `1024`.
Surface ID принимает ноль и широкий разреженный набор ненулевых значений вплоть
до 222.

Семантические имена отдельных flag bits и смысл ID пока не восстановлены.
Корреляции с surface type недостаточно, чтобы назвать их collision response,
footstep bank, sector или material index. Viewer поэтому показывает точные
числа и гистограммы, но не присваивает им вымышленные enum-имена.

## Межкорпусное сравнение

- `pc-working` и `pc-pristine`: все 95/95 ресурсов имеют одинаковые
  последовательности полных `spMeshBV` payload;
- PC/PS2: 69 общих канонических путей, у 60 совпадает число объектов;
- по пути и ordinal сопоставлены 3 295 объектов;
- field 0 побайтно совпадает в 3 171 паре;
- наличие field 1 совпадает в 3 281 паре;
- полный face-data payload совпадает в 3 269 паре.

Несовпадения являются различиями конкретных platform resources, а не новым
layout: обе стороны каждой пары отдельно проходят тот же строгий decoder.

## Свидетельства executable

Pristine PC `WinxClub.exe`:

- reader `spMeshBV` — `0x00438490..0x0043866A`;
- writer `spMeshBV` — `0x00438680..0x00438924`;
- aggregate reader/writer face array — `0x0047D5E0` / `0x0047D530`;
- reader/writer одного `wxFaceData` — `0x005A5320` / `0x005A4FE0`;
- surface-type string converter — `0x005A0660..0x005A0774`;
- class registration содержит `0x3F453DE7`, base `spBoundingVolume`
  `0x21CC76AF` и строку `spMeshBVSerializer`.

PS2 `SLES_532.19` независимо подтверждает ту же схему:

- reader `spMeshBV` — `0x00189360..0x00189558`;
- writer `spMeshBV` — `0x00189570..0x00189758`;
- строки `esfMeshBV`/`esfMeshBVFaceData` — `0x0044C780..0x0044C8C0`;
- `wxFaceData` и diagnostics трёх членов — `0x00460770..0x00460AE0`;
- присутствуют все девять именованных surface-строк.

PC x86 и PS2 MIPS serializer являются независимым подтверждением типов полей;
корпус дополнительно доказывает cardinality, default omission и точные границы.

## Viewer и база

`SmoMeshBoundingVolumeDecoder` строго проверяет обе верхние формы, версию,
reserved words, размер массивов, индексы, конечность координат, class ID и
полное завершение каждого `wxFaceData`. Старый collision overlay теперь получает
геометрию через этот же decoder, поэтому больше не игнорирует неизвестный хвост.

Флажок «Показывать восстановленные поля» выводит для field 0 версию, числа
треугольников/вершин, bounds и число вырожденных граней. Для field 1 он показывает
class ID, число записей и гистограммы surface type с именами, flags и surface ID.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spMeshBV
```

повторно читает полные directory/PCK payload, проверяет оба executable и
идемпотентно записывает два варианта, 10 513 назначений, 14 116 аннотаций полей
и четыре evidence-записи. Отдельный read-only отчёт строится командой
[`analyze_smo_mesh_bv.py`](../../research/analyze_smo_mesh_bv.py).

## Открытые вопросы

1. Восстановить имена и потребителей отдельных битов `m_uFlags`.
2. Определить область уникальности и runtime-роль `m_uSurfaceID`.
3. Проверить изменение surface metadata и геометрии в native runtime до
   включения безопасной записи этих полей.

Связанные partition-классы уже разобраны. Быстрые игровые проверки flags,
surface ID и surface type входят в
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
