# `spMatColorControllerSerializer`

Обновление PC CP25–27: [runtime/common codec](native-pc-material-color.md),
[material references](native-pc-material-color-graph.md),
[реальный payload roundtrip](native-pc-renderer-scene.md). Исполняемый portable
codec использует восстановленные Scalar/ColorEval и общее ядро sections/refs.
Ниже исторический schema-only этап; protected controller ctor всё ещё открыт.

Статус: восстановлены идентичность, RTTI/lifetime, раздельный PC/PS2 ABI и
подтверждённая структура пяти evaluator-секций. Portable-класс намеренно не
реализует формулы evaluator-ов и реальный stream codec до реконструкции самих
`spColorFuncEval`/`spFunctionEval`.

## Идентичность

Обе платформы регистрируют `spMatColorControllerSerializer` с Class ID
`0x0F881A36`, прямым base `spSerializer` (`0x42429877`) и target
`spMaterialColorController` (`0x4C633E85`). Наличие полной PS2-реализации важно:
в исследованном PS2-корпусе нет ни одного объекта этого target-класса, но это
политика ресурсов, а не отсутствие подсистемы в движке.

Точный исходный `.cpp` path не найден. PS2 factory выделяет `0x14` байт, а PC
destructor/vtables подтверждают такой же observed extent без derived storage.

## Состав сериализации

Собственная секция имеет один внешний field ID 0, названный в диагностике
`esfMaterialColorController`. Внутри reader и writer вызывают специализированные
helper-сериализаторы в фиксированном порядке:

| Порядок | Роль | Helper | Target offset PC/PS2 |
|---:|---|---|---:|
| 0 | ambient | color-functional evaluator | `+0x68` |
| 1 | diffuse | color-functional evaluator | `+0xB8` |
| 2 | specular | color-functional evaluator | `+0x108` |
| 3 | emissive | color-functional evaluator | `+0x158` |
| 4 | alpha | functional evaluator | `+0x1A8` |

Эти пять смещений и разделение 4+1 независимо видны в x86 и MIPS-коде. Reader
обеих платформ требует ненулевой target, обрабатывает field 0 и передаёт
неизвестные ID общему skip-пути. Writer всегда открывает внешнюю секцию и пишет
все пять evaluator-ов; уже сами helper-ы подавляют значения по умолчанию.

Portable `EvaluatorPlan` фиксирует только доказанные роли, helper-kind и offsets.
Он не объявляет layout evaluator-ов частью layout serializer-а. Полная
read-only статистика полей helper-ов находится в
[`smo-class-sp-material-color-controller.md`](smo-class-sp-material-color-controller.md).

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D3090..0x006D30B5`, record `0x0075EE30`;
- registration getter `0x004411C0`, target hook `0x004411F0`;
- protected factory entry `0x00441200`, destructor `0x004411D0`, clone
  `0x00441270`, deleting destructor `0x004412C0`;
- reader `0x004412E0`, writer `0x00441740`;
- primary/interface vtables `0x006E2064` / `0x006E2058`;
- class string `0x006E241C`, outer-field diagnostics
  `0x006EA030..0x006EA0E0`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483210`, record `0x004AA090`;
- registration getter `0x00187250`, reader `0x00187260`, writer
  `0x00187530`, target hook `0x001878D0`;
- success/finalize slot `0x00187520`;
- deleting destructor `0x001878E0`, clone `0x00187950`, factory
  `0x00187A30`, exact allocation `0x14`;
- read/finalize/write thunks `0x00187AA0`, `0x00187AB0`, `0x00187AC0`;
- primary/interface vtable headers `0x0048F230` / `0x0048F254`;
- class string `0x0044BC90`, outer-field diagnostics
  `0x00452240..0x004522F0`.

## Неизвестное

- исходные header/source paths и оригинальные имена методов;
- полные native layouts controller-а и двух evaluator-классов;
- точная формула каждого evaluator type и допустимые диапазоны параметров;
- семантика отдельного success/finalize slot;
- поведение rollback при ошибке одной из пяти вложенных секций;
- почему поставляемый PS2-корпус не использует класс;
- контролируемый in-game mutation test.

Следующий класс по текущему serializer-ряду — `spLightControllerSerializer`.
