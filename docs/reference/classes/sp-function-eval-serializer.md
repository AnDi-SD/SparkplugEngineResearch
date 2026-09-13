# spFunctionEvalSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spFunctionEvalSerializer](../../../Sparkplug/Code/Sparkplug/spFunctionEvalSerializer.h).

Статус: подтверждены исходное имя класса, identity, RTTI/lifetime, раздельный
PC/PS2 ABI и полная шестиполевая scalar grammar. Runtime-математика target-а
намеренно не восстановлена по одним лишь данным serializer-а.

## Field schema

| ID | Значение | Wire | Default | Target offset |
| ---: | --- | --- | ---: | ---: |
| 0 | FunctionType | UInt32 | `0` | `+0x34` |
| 1 | Frequency | Float32 | `1` | `+0x14` |
| 2 | Amplitude | Float32 | `1` | `+0x1C` |
| 3 | XOffset | Float32 | `0` | `+0x20` |
| 4 | YOffset | Float32 | `0` | `+0x24` |
| 5 | Pitch | Float32 | `0` | `+0x28` |

Шесть полей и их defaults совпадают с ранее строго декодированными evaluator-
блоками `spUVController`. Наблюдаемые raw FunctionType `0..8` не превращены в
семантический enum: serializer хранит число, но не объясняет формулу.

## Неизвестное

- оригинальные header/source paths;
- точное зарегистрированное имя target-типа для ID `0x9450E590`;
- полный target layout, constructor defaults и ownership;
- значения `FunctionType` и runtime-формулы;
- zero/NaN/Inf semantics reciprocal frequency;
- time source, wrap/clamp behavior и контролируемый in-game mutation test.
