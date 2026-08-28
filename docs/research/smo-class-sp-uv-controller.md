# `spUVController` (`0x1C0053D6`)

Статус: общий PC/PS2 layout и все наблюдаемые payload полностью декодируются,
но остаются read-only до проверки изменённого файла в игре. Числовые значения
`FunctionType` сохранены как raw enum: назначение отдельных значений 0–8 кодом
serializer не раскрывается, поэтому им пока не присвоены предположительные имена.

## Распространённость

| Корпус | Объекты | Уникальные SMO | Физические вхождения | Размер объекта |
|---|---:|---:|---:|---:|
| `pc-pristine` | 1 607 | 186 | 1 607 | 78–133 |
| `pc-working` | 1 607 | 186 | 1 607 | 78–133 |
| `ps2-pristine` | 1 491 | 162 | 3 663 | 78–133 |

Все 4 705 уникальных записей трёх корпусов безымянны и являются прямыми
дочерними объектами `spMaterialData`. Больший physical count PS2 вызван
повторными вхождениями одинаковых ресурсов в PCK, а не дополнительными формами.

## Внешнее поле

Собственная serializer-секция всегда содержит ровно field 0 и terminator:

| Field | Семантика | Layout |
|---:|---|---|
| 0 | `uv_controller.transform_evaluators` | вложенный ресурс `spTransFunctionEval` |

Полный размер объекта равен `14 + payloadSize`: восемь байт class/SBOO,
пятибайтовый `E0`/UInt32 header field 0, payload и однобайтовый terminator.

Сам payload имеет вид:

```text
A0 <nested-size:UInt8>
  FunctionalEvaluator translation X
  FunctionalEvaluator translation Y
  FunctionalEvaluator translation Z
  FunctionalEvaluator scale X
  FunctionalEvaluator scale Y
  FunctionalEvaluator scale Z
  FunctionalEvaluator rotation
  Vector3 UV pivot
  Vector3 rotation axis
00  // конец вложенного ресурса
```

Порядок двух векторов подтверждён одновременно данными и serializer: в SMO
сначала записывается UV pivot, затем rotation axis. Во всём корпусе rotation axis
равен `(0,0,1)`. Найдены четыре pivot: `(0,0,0)`, `(0.5,0.5,0)`, `(1,2,0)` и
`(1.5,1.5,0)`.

## `FunctionalEvaluator`

Каждый из семи evaluator-блоков является собственной последовательностью
компактных полей и заканчивается `00`.

| Field | Тип | Значение по умолчанию |
|---:|---|---:|
| 0 | `UInt32 FunctionType` | `0` |
| 1 | `Single Frequency` | `1` |
| 2 | `Single Amplitude` | `1` |
| 3 | `Single XOffset` | `0` |
| 4 | `Single YOffset` | `0` |
| 5 | `Single Pitch` | `0` |

Каждое явно записанное значение занимает пять байт: однобайтовый fixed-4
header и четыре байта значения. Serializer опускает поля, равные default.
Поэтому полный payload имеет формулу:

```text
payloadSize = 34 + 5 * explicitEvaluatorFields
objectSize  = 48 + 5 * explicitEvaluatorFields
```

Наблюдаемые размеры и число объектов в объединённом индексе трёх корпусов:

| Object | Payload | Явных evaluator-полей | Объекты |
|---:|---:|---:|---:|
| 78 | 64 | 6 | 3 |
| 83 | 69 | 7 | 18 |
| 88 | 74 | 8 | 633 |
| 93 | 79 | 9 | 1 123 |
| 98 | 84 | 10 | 1 092 |
| 103 | 89 | 11 | 699 |
| 108 | 94 | 12 | 639 |
| 113 | 99 | 13 | 57 |
| 118 | 104 | 14 | 342 |
| 128 | 114 | 16 | 51 |
| 133 | 119 | 17 | 48 |

Форма с 15 явно записанными полями (`payload 109`, object 123) технически
допустима общей схемой, но в доступном корпусе не встречается. Одиннадцать
размеров — это разреженные формы одного layout, а не одиннадцать подтипов.

Наблюдаемые raw `FunctionType` по назначению evaluator:

| Evaluator | Значения |
|---|---|
| translation X | `0,1,2,3,4,6,8` |
| translation Y | `0,1,2,3,4,6,8` |
| translation Z | `0` |
| scale X | `0,1,6` |
| scale Y | `0,1,3,6` |
| scale Z | `0` |
| rotation | `0,1,2,3,4,5,6,8` |

Это значения параметра, а не discriminator подтипа `spUVController`.

## Сравнение корпусов

`pc-working` и `pc-pristine` совпадают по полному мультимножеству payload во
всех 186 одноимённых ресурсах. Из 162 общих PC/PS2 ресурсов полностью совпадают
157. Пять ресурсов имеют одинаковый layout, но различающиеся параметры:

- `SFX/Bloom_lightorb03.smo`;
- `SFX/Bloom_spiral_ray.smo`;
- `SFX/Bloom_tidalflame_b.smo`;
- `SFX/firedragon.smo`;
- `SFX/spitting_goo.smo`.

Это платформенные различия значений, а не отдельный PS2-формат. Всего после
чтения полных payload, а не только 48-байтовых preview базы, найдено 148 разных
байтовых представлений.

## Свидетельства executable

Pristine PC `WinxClub.exe`:

- wrapper load/serialize: `0x00440BE0..0x00440FD7`;
- field 0 передаётся вложенному serializer `spTransFunctionEval`;
- строки `GetTranslationFunction(eaxX/Y/Z)`, `GetScaleFunction(eaxX/Y/Z)`,
  `GetRotationFunction`, `GetUVPivot` и `GetRotationAxis` находятся в одном
  serializer-блоке;
- class strings: `spUVController`, `spUVControllerSerializer`,
  `spTransFunctionEval`, `spTransFunctionEvalSerializer`.

PS2 `SLES_532.19` независимо подтверждает тот же порядок:

- nested load: `0x00187AE0..0x00187EAC`;
- nested serialize: `0x00187EC0..0x001880F8`;
- wrapper: `0x00188350..0x00188640`;
- MIPS serializer семь раз вызывает один `FunctionalEvaluator` serializer,
  затем пишет два `Vector3` в порядке UV pivot, rotation axis.

## Viewer и база

`SmoUvControllerDecoder` строго проверяет framing вложенного ресурса, семь
evaluator-terminator, два вектора и конечный terminator. Read-only inspector
показывает все девять компонентов при включённом флажке восстановленных полей.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spUVController
```

перечитывает полные payload непосредственно из directory SMO и PCK offsets,
декодирует 4 705 объектов, записывает JSON в `direct_fields`, common definition,
один структурный вариант `uv_transform_sparse`, 4 705 назначений и четыре
evidence-записи. Редактирование намеренно не включено.
