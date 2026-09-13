# spUVControllerSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTransFunctionEval](../../../Sparkplug/Code/Sparkplug/spTransFunctionEval.h), [spUVControllerSerializer](../../../Sparkplug/Code/Sparkplug/spUVControllerSerializer.h).

Статус: восстановлены identity, RTTI/lifetime, раздельный PC/PS2 ABI и полностью
совпадающая структурная схема wrapper-а над `spTransFunctionEval`. Portable-класс
пока описывает порядок и offsets, но не выдаёт частично понятую математику
functional evaluator-ов за готовый codec.

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

## Неизвестное

- оригинальные header/source paths и имена большинства методов;
- полный layout/lifecycle `spUVController` за пределами подтверждённых offsets;
- исходный C++ interface временного `spTransFunctionEvalSerializer`;
- точные имена и значения enum `FunctionType`;
- все runtime-формулы evaluator-ов, time source, wrap/clamp и NaN semantics;
- error/rollback behavior вложенного reader-а;
- контролируемый in-game mutation test.
