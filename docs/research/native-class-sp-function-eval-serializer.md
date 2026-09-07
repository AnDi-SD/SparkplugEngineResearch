# `spFunctionEvalSerializer`

PC checkpoint21: [актуальный runtime/codec](native-pc-function-eval.md)
подтвердил зарегистрированное имя target, factory38, все finite scalar режимы,
shared RNG и actual codec;21 exact сравнений. Ниже сохранён исходный разбор
grammar и его тогдашние unknowns. Они не отменяют последующее PC evidence;
PS2 runtime из него автоматически не выводится.

Статус: подтверждены исходное имя класса, identity, RTTI/lifetime, раздельный
PC/PS2 ABI и полная шестиполевая scalar grammar. Runtime-математика target-а
намеренно не восстановлена по одним лишь данным serializer-а.

## Идентичность и размер

Обе сборки регистрируют `spFunctionEvalSerializer` с Class ID `0x1D2A151D`,
прямым base `spSerializer` (`0x42429877`) и target Class ID `0x9450E590`.
Диагностические строки называют target `spFunctionEval`; отдельное RTTI-имя
target-а в этом проходе не доказано. PS2 factory выделяет `0x14` байт, PC vtables
подтверждают тот же observed extent без derived storage.

## Field schema

| ID | Значение | Wire | Default | Target offset |
|---:|---|---|---:|---:|
| 0 | FunctionType | UInt32 | `0` | `+0x34` |
| 1 | Frequency | Float32 | `1` | `+0x14` |
| 2 | Amplitude | Float32 | `1` | `+0x1C` |
| 3 | XOffset | Float32 | `0` | `+0x20` |
| 4 | YOffset | Float32 | `0` | `+0x24` |
| 5 | Pitch | Float32 | `0` | `+0x28` |

Writer пропускает каждое значение, равное default, сохраняя порядок ID. Reader
при чтении Frequency дополнительно записывает `1.0 / frequency` по `+0x18`.
Проверки нуля перед делением в обеих native-реализациях не видно. Finalize всегда
успешен: relationships отсутствуют.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D41B0..0x006D41D5`, register call
  `0x006D41D0`, record `0x00761338`;
- registration getter `0x0047ECE0`, target hook `0x0047ED10`;
- protected factory entry `0x0047ED20`, destructor `0x0047ECF0`, clone
  `0x0047ED90`, deleting destructor `0x0047EDE0`;
- reader `0x0047EE00`, writer `0x0047F220`, shared successful finalize
  `0x005A7DB0`;
- primary/interface vtables `0x006EB458` / `0x006EB44C`;
- class string `0x006EB6B0`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483190`, record `0x004A9FD0`;
- registration getter `0x00186100`, target hook `0x00186670`;
- reader `0x00186110`, finalize `0x00186390`, writer `0x001863A0`;
- deleting destructor `0x00186680`, clone `0x00186730`, factory
  `0x00186810`, exact allocation `0x14`;
- read/finalize/write thunks `0x00186880/0x00186890/0x001868A0`;
- primary/interface vtable headers `0x0048F170` / `0x0048F194`;
- class string `0x0044B1E0`.

## Сверка с корпусом

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

Следующий соседний кандидат — `spColorFunctionEvalSerializer`, используемый
четыре раза material-color controller-ом; его имя и identity должны быть сначала
подтверждены обеими сборками.
