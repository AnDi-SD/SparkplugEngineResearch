# `spColorFuncEvalSerializer`

Обновление PC CP24: [реальный runtime и общий codec](native-pc-color-functions.md)
проверены отдельно от первоначального schema-only описания ниже. PC byte blend,
factory/blank clone,20 exact source/native comparisons; это не PS2 credit.

Статус: подтверждены native class name, identity, RTTI/lifetime, раздельный
PC/PS2 ABI и полная восьмиполевая scalar grammar. Различие platform default color
сохранено явно, как и у `spLightControllerSerializer`.

## Идентичность и размер

Обе сборки регистрируют `spColorFuncEvalSerializer` с Class ID `0x2CC46B90`,
прямым base `spSerializer` (`0x42429877`) и target `spColorFuncEval`
(`0x0BC70FE7`). PS2 factory выделяет `0x14` байт; PC vtables подтверждают тот же
observed extent без derived storage.

## Field schema

| ID | Имя из diagnostics | Wire | Default | Target offset |
|---:|---|---|---:|---:|
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

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D4180..0x006D41A5`, register call
  `0x006D41A0`, record `0x007612D8`;
- registration getter `0x0047E200`, target hook `0x0047E230`;
- protected factory entry `0x0047E240`, destructor `0x0047E210`, clone
  `0x0047E2B0`, deleting destructor `0x0047E300`;
- reader `0x0047E320`, writer `0x0047E850`, shared successful finalize
  `0x005A7DB0`;
- primary/interface vtables `0x006EB13C` / `0x006EB130`;
- class string `0x006EB430`, default color at `0x0073FE98`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483150`, record `0x004A9F70`;
- registration getter `0x001857C0`, target hook `0x00185EC0`;
- reader `0x001857D0`, finalize `0x00185AD0`, writer `0x00185AE0`;
- deleting destructor `0x00185ED0`, clone `0x00185F80`, factory
  `0x00186060`, exact allocation `0x14`;
- read/finalize/write thunks `0x001860D0/0x001860E0/0x001860F0`;
- primary/interface vtable headers `0x0048F110` / `0x0048F134`;
- class string `0x0044AE80`, default color at `0x00476CB0`.

## Сверка с корпусом

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
