# spColorFuncEvalSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spColorFuncEvalSerializer](../../../Sparkplug/Code/Sparkplug/spColorFuncEvalSerializer.h).

Статус: подтверждены native class name, identity, RTTI/lifetime, раздельный
PC/PS2 ABI и полная восьмиполевая scalar grammar. Различие platform default color
сохранено явно, как и у `spLightControllerSerializer`.

## Field schema

| ID | Имя из diagnostics | Wire | Default | Target offset |
| ---: | --- | --- | ---: | ---: |
| 0 | `esfColorFuncEvalColor1` | ARGB | platform color | `+0x10` |
| 1 | `esfColorFuncEvalColor2` | ARGB | platform color | `+0x14` |
| 2 | `esfColorFuncEvalType` | UInt32 | `0` | `+0x4C` |
| 3 | `esfColorFuncEvalFrequency` | Float32 | `1` | `+0x2C` |
| 4 | `esfColorFuncEvalAmplitude` | Float32 | `1` | `+0x34` |
| 5 | `esfColorFuncEvalXOffset` | Float32 | `0` | `+0x38` |
| 6 | `esfColorFuncEvalYOffset` | Float32 | `0` | `+0x3C` |
| 7 | `esfColorFuncEvalPitch` | Float32 | `0` | `+0x40` |

Color default равен `0xFF000000` на PC и `0x00000000` на PS2. Writer подавляет
default-поля в порядке ID. Reader при Frequency также сохраняет reciprocal по
`+0x30`. Relationships отсутствуют, finalize всегда успешен.

Схема совпадает с четырьмя color evaluator-блоками единственного найденного
`spMaterialColorController`: там явно присутствует только Frequency `0.1`, а
остальные поля подавлены как defaults. Это подтверждает grammar, но не задаёт
пределы движка и не объясняет runtime-интерполяцию цветов.

## Неизвестное

- оригинальные header/source paths;
- полный target lifecycle/layout и constructor defaults;
- причина различия default alpha между PC и PS2;
- значения function type и точная runtime-интерполяция ARGB;
- zero/NaN/Inf semantics frequency и цветовых вычислений;
- контролируемый in-game mutation test.

После закрытия обоих evaluator leaf-serializer-ов локальная цепочка
`spMaterialColorController`/`spUVController` структурно замкнута. Следующий класс
выбирается из соседних controller serializers по подтверждённой регистрации.
