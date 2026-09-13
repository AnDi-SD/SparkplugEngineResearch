# PC ParticleSystem: параметры, writer и первая целая сцена

## Выполненные оригинальные функции

`48D8D0` создаёт `spParticleSystem` (264 bytes, vtable `6EC40C`), constructor
`48D6F0` вызывает настоящий lazy manager `45A530` через global `75DB84` и
регистрирует объект. `49BBB0` создаёт serializer (20 bytes). Его secondary
table `6ED47C`: writer `49C8D0`, index `49BC90`, reader `49BCE0`.
RTTI initializer `6D4AA0` задаёт serializer ID `047F310F` и parent
`spRenderableSerializer`; target ID — `5AFA1A4F`.

## Уточнения поведения

[ReadPayloadForAnalysis](../../../Sparkplug/Code/Sparkplug/spParticleSystemSerializer.cpp)
после чтения конечных параметров и одной полной emission region вызывает общий
`InitializeForAnalysis`. [Реализация инициализации](../../../Sparkplug/Code/Sparkplug/spParticleSystemRuntime.cpp)
поддерживает non-looping пул без активных записей и looping пул с начальным
испусканием, семью типами region и общим состоянием генератора случайных чисел.
Looping требует живого RenderNode. Сохраняется оригинальная обработка последнего
пакета через остаток: при capacity=128 начальное испускание оставляет весь пул
свободным; подробнее — [граница пакета 128](particle-loop-init-counts.md).

Ограничения host задаются явно: capacity 2..1024, положительный конечный rate,
полные конечные параметры region, ограниченный диапазон углов и координат.
Reader отвергает неправильные размеры полей, повторную region, нечисловой direction
и ссылку на RenderNode без владельца графа. Повторная инициализация и clone
не восстановлены. Writer поддерживает также constructor defaults и flags;
наличие writer не означает готовность полного SMO SaveResources.

Отдельные общие аналитические реализации описывают
[обновление существующих записей](particle-cpu-update.md),
[расчёт запроса испускания](particle-emission-budget.md),
[CPU draw](ballistic-cpu-draw.md) и [ballistic shader](ballistic-shader.md).
Они не заменяют полный runtime класса: переиспускание в ходе update,
modifier callbacks и связь всех стадий с игровым кадром остаются отдельными границами.
