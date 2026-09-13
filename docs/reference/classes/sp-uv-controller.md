# spUVController

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spUVController](../../../Sparkplug/Code/Sparkplug/spUVController.h).

## Внешнее поле

Собственная serializer-секция всегда содержит ровно field 0 и terminator:

| Field | Семантика | Layout |
| ---: | --- | --- |
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

## `FunctionalEvaluator`

Каждый из семи evaluator-блоков является собственной последовательностью
компактных полей и заканчивается `00`.

| Field | Тип | Значение по умолчанию |
| ---: | --- | ---: |
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

| Object | Payload | Явных evaluator-полей | Объекты |
| ---: | ---: | ---: | ---: |
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

Наблюдаемые raw `FunctionType` по назначению evaluator:

| Evaluator | Значения |
| --- | --- |
| translation X | `0,1,2,3,4,6,8` |
| translation Y | `0,1,2,3,4,6,8` |
| translation Z | `0` |
| scale X | `0,1,6` |
| scale Y | `0,1,3,6` |
| scale Z | `0` |
| rotation | `0,1,2,3,4,5,6,8` |

Это значения параметра, а не discriminator подтипа `spUVController`.

## Viewer и база

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spUVController
```
