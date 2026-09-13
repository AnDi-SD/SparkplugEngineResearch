# spMaterialColorController

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMaterialColorController](../../../Sparkplug/Code/Sparkplug/spMaterialColorController.h).

## Внешний layout

| Относительное смещение | Размер | Значение |
| ---: | ---: | --- |
| `0x00` | 4 | class ID `0x4C633E85` |
| `0x04` | 4 | `SBOO` |
| `0x08` | 5 | field 0, raw header `0xE0`, UInt32 size `40` |
| `0x0D` | 40 | пять последовательных evaluator-секций |
| `0x35` | 1 | terminator собственной serializer-секции |

## Вложенные evaluator-секции

Порядок доказан PC serializer-кодом:

1. ambient `ColorFunctionalEvaluator`;
2. diffuse `ColorFunctionalEvaluator`;
3. specular `ColorFunctionalEvaluator`;
4. emissive `ColorFunctionalEvaluator`;
5. alpha `FunctionalEvaluator`.

Первые четыре секции имеют одинаковые шесть байт `63 CD CC CC 3D 00`:
field 3 (`frequency`) равен `0.1`, затем идёт terminator. Alpha-секция:
`61 CD CC CC 3D 62 00 00 00 00 64 00 00 80 3F 00`, то есть
`frequency=0.1`, `amplitude=0`, `yOffset=1`, terminator.

### `ColorFunctionalEvaluator`

| Field | Имя из executable | Тип | Default | Наблюдается |
| ---: | --- | --- | ---: | ---: |
| 0 | `esfColorFuncEvalColor1` | ARGB UInt32 | `0xFF000000` | нет |
| 1 | `esfColorFuncEvalColor2` | ARGB UInt32 | `0xFF000000` | нет |
| 2 | `esfColorFuncEvalType` | UInt32 | 0 | нет |
| 3 | `esfColorFuncEvalFrequency` | Single | 1 | **0.1** |
| 4 | `esfColorFuncEvalAmplitude` | Single | 1 | нет |
| 5 | `esfColorFuncEvalXOffset` | Single | 0 | нет |
| 6 | `esfColorFuncEvalYOffset` | Single | 0 | нет |
| 7 | `esfColorFuncEvalPitch` | Single | 0 | нет |

### `FunctionalEvaluator` alpha

| Field | Имя из executable | Тип | Default | Наблюдается |
| ---: | --- | --- | ---: | ---: |
| 0 | `esfFunctionEvalType` | UInt32 | 0 | нет |
| 1 | `esfFunctionEvalFrequency` | Single | 1 | **0.1** |
| 2 | `esfFunctionEvalAmplitude` | Single | 1 | **0** |
| 3 | `esfFunctionEvalXOffset` | Single | 0 | нет |
| 4 | `esfFunctionEvalYOffset` | Single | 0 | **1** |
| 5 | `esfFunctionEvalPitch` | Single | 0 | нет |
