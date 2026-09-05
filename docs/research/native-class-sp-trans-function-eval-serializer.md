# `spTransFunctionEvalSerializer`

Статус: самостоятельная RTTI-запись, lifecycle, раздельный PC/PS2 ABI и
структурная grammar подтверждены. Класс отделён от `spUVControllerSerializer`:
UV-wrapper создаёт обычный временный экземпляр этого serializer-а и передаёт ему
embedded target, а не вызывает безымянный локальный helper.

## Идентичность

Обе сборки регистрируют `spTransFunctionEvalSerializer` с Class ID `0x2AE96657`,
прямым base `spSerializer` (`0x42429877`) и target `spTransFunctionEval`
(`0x491432F0`). PS2 factory выделяет `0x14` байт; PC primary/interface vtables
подтверждают такой же observed extent без derived storage.

## Field grammar и target offsets

Writer создаёт единственный field 0 wire-типа 5. Внутри него строго последовательно
обрабатываются семь functional evaluator-ов:

| Роль | Offset |
|---|---:|
| translation X/Y/Z | `+0x10/+0x48/+0x80` |
| scale X/Y/Z | `+0xB8/+0xF0/+0x128` |
| rotation | `+0x178` |

После них идут `Vector3` UV pivot по `+0x160` и rotation axis по `+0x16C`.
Reader принимает только field ID 0; отдельный finalize сразу успешен, потому что
relationship-объектов нет. В portable-код перенесены порядок, wire-type и offsets,
но не сами ещё не восстановленные функции вычисления.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D4150..0x006D4175`, register call
  `0x006D4170`, record `0x00761278`;
- registration getter `0x0047DAA0`, target hook `0x0047DAD0`;
- protected factory entry `0x0047DAE0`, destructor `0x0047DAB0`, clone
  `0x0047DB50`, deleting destructor `0x0047DBA0`;
- reader `0x0047DBC0`, writer `0x0047DE80`, shared successful finalize
  `0x005A7DB0`;
- primary/interface vtables `0x006EACC0` / `0x006EACB4`;
- class string `0x006EB110`.

Часть PC entry-кода содержит защитные непрямые переходы, поэтому внутренняя
grammar не выводилась из линейной декомпиляции PC в одиночку: адреса reader/writer
закреплены vtable, а содержательная схема независимо читается в PS2-коде и
совпадает с PC/PS2 корпусом.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483250`, record `0x004AA0F0`;
- registration getter `0x00187AD0`, target hook `0x00188100`;
- reader `0x00187AE0`, finalize `0x00187EB0`, writer `0x00187EC0`;
- deleting destructor `0x00188110`, clone `0x001881C0`, factory
  `0x001882A0`, exact allocation `0x14`;
- read/finalize/write thunks `0x00188310/0x00188320/0x00188330`;
- primary/interface vtable headers `0x0048F290` / `0x0048F2B4`;
- class string `0x0044C1B0`.

## Неизвестное

- оригинальные header/source paths и исходные имена методов;
- полный `spTransFunctionEval` layout за пределами подтверждённых offsets;
- field names из original enum и публичный interface getter-ов;
- `FunctionType` enum и точные runtime-формулы;
- default/NaN/time/wrap semantics и обработка ошибок;
- контролируемый in-game mutation test.

Следующий маленький кандидат — serializer отдельного functional evaluator-а,
который вызывается семь раз из этого класса; его identity и границы нужно сначала
найти независимо, не предполагая имя по роли call target-а.
