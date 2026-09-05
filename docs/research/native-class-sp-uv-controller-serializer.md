# `spUVControllerSerializer`

Статус: восстановлены identity, RTTI/lifetime, раздельный PC/PS2 ABI и полностью
совпадающая структурная схема wrapper-а над `spTransFunctionEval`. Portable-класс
пока описывает порядок и offsets, но не выдаёт частично понятую математику
functional evaluator-ов за готовый codec.

## Идентичность и размер

Обе сборки регистрируют `spUVControllerSerializer` с Class ID `0x591224D0`,
прямым base `spSerializer` (`0x42429877`) и target `spUVController`
(`0x1C0053D6`). PS2 factory выделяет ровно `0x14` байт; PC vtables и lifecycle
подтверждают тот же observed extent без derived storage. Точный исходный путь
пока не найден.

## Структура сериализации

Wrapper всегда создаёт один внешний field 0 типа 7. Его payload обрабатывается
временным `spTransFunctionEvalSerializer`, которому передаётся подобъект target-а
по offset `+0x4C`. Внутри, в фиксированном порядке, идут:

1. translation X, Y, Z — functional evaluator offsets `+0x10/+0x48/+0x80`;
2. scale X, Y, Z — offsets `+0xB8/+0xF0/+0x128`;
3. rotation — offset `+0x178`;
4. UV pivot `Vector3` — offset `+0x160`;
5. rotation axis `Vector3` — offset `+0x16C`.

Соответствующие абсолютные offsets в `spUVController` равны
`+0x5C/+0x94/+0xCC/+0x104/+0x13C/+0x174/+0x1C4`, затем `+0x1AC/+0x1B8`.
Relationship pass отсутствует: PC использует общий success helper, PS2 имеет
локальную функцию, сразу возвращающую true.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D3030..0x006D3055`, register call
  `0x006D3050`, record `0x0075ED70`;
- registration getter `0x00440AC0`, target hook `0x00440AF0`;
- protected factory entry `0x00440B00`, destructor `0x00440AD0`, clone
  `0x00440B70`, deleting destructor `0x00440BC0`;
- reader `0x00440BE0`, writer `0x00440D70`, common successful finalize
  `0x005A7DB0`;
- primary/interface vtables `0x006E1E6C` / `0x006E1E60`;
- class string `0x006E1F9C`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483290`, record `0x004AA150`;
- registration getter `0x00188340`, target hook `0x00188640`;
- reader `0x00188350`, no-relationship finalize `0x00188460`, writer
  `0x00188470`;
- deleting destructor `0x00188650`, clone `0x001886C0`, factory
  `0x001887A0`, exact allocation `0x14`;
- read/finalize/write thunks `0x00188810/0x00188820/0x00188830`;
- primary/interface vtable headers `0x0048F2F0` / `0x0048F314`;
- nested read/write `0x00187AE0` / `0x00187EC0`, class string `0x0044C380`.

## Сверка с корпусом

Read-only декодер ранее подтвердил 4 705 уникальных записей из PC/PS2 корпусов.
Все имеют один field 0, семь evaluator-блоков и два `Vector3` в подтверждённом
native-кодом порядке. Одиннадцать наблюдаемых размеров объясняются default
suppression внутри evaluator-ов, а не вариантами layout.

## Неизвестное

- оригинальные header/source paths и имена большинства методов;
- полный layout/lifecycle `spUVController` за пределами подтверждённых offsets;
- исходный C++ interface временного `spTransFunctionEvalSerializer`;
- точные имена и значения enum `FunctionType`;
- все runtime-формулы evaluator-ов, time source, wrap/clamp и NaN semantics;
- error/rollback behavior вложенного reader-а;
- контролируемый in-game mutation test.

Следующая локальная зависимость — `spTransFunctionEvalSerializer`: её нужно
отделить от временного helper-а wrapper-а, подтвердить identity и ABI обеих
платформ и только затем переносить собственную field grammar.
