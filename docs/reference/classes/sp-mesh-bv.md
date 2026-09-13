# spMeshBV

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMeshBV](../../../Sparkplug/Code/Sparkplug/spMeshBV.h), [wxFaceData](../../../Winx/Code/wxFaceData.h).

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
| ---: | --- | --- |
| 1 | `UInt32` | index-buffer type, во всех объектах `2` (triangle list) |
| 2 | `UInt32` | число треугольников `T` |
| 3 | `UInt32` | index format flags, известное значение — ноль (UInt16) |
| 4 | `UInt16[T*3]` | triangle-list indices |
| 5 | `UInt32` | vertex component flags, известное значение — ноль (позиции) |
| 6 | `UInt32` | число вершин `V` |
| 7 | `UInt32` | vertex buffer flags, известное значение — ноль |
| 8 | `Vector3[V]` | позиции `Single X/Y/Z` |

Формула размера payload:

```text
24 + T * 6 + V * 12 bytes
```

| Треугольники | Вершины | Вырожденные треугольники |
| ---: | ---: | ---: |
| 98 927 | 83 440 | 21 |
| 93 211 | 78 674 | 21 |

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

Непустые члены встречаются:

## Тип поверхности

PC converter и совпадающий набор PS2-строк подтверждают значения:

| Значение | Имя executable | PS2 pristine |
| ---: | --- | ---: |
| 0 | fallback/unspecified | 6 950 |
| 1 | `stone` | 16 592 |
| 2 | `dirt` | 1 276 |
| 3 | `grass` | 4 133 |
| 4 | `water` | 648 |
| 5 | `snow` | 4 376 |
| 6 | `swamp` | 0 |
| 7 | `mud` | 167 |
| 8 | `deepwater` | 12 |
| 9 | `carpet` | 686 |

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

## Viewer и база

Флажок «Показывать восстановленные поля» выводит для field 0 версию, числа
треугольников/вершин, bounds и число вырожденных граней. Для field 1 он показывает
class ID, число записей и гистограммы surface type с именами, flags и surface ID.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spMeshBV
```

## Открытые вопросы

1. Восстановить имена и потребителей отдельных битов `m_uFlags`.
2. Определить область уникальности и runtime-роль `m_uSurfaceID`.
3. Проверить изменение surface metadata и геометрии в native runtime до
   включения безопасной записи этих полей.
