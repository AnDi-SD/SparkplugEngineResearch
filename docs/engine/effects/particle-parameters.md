# PC ParticleSystem: параметры, writer и первая целая сцена

## Выполненные оригинальные функции

`48D8D0` создаёт `spParticleSystem` (264 bytes, vtable `6EC40C`), constructor
`48D6F0` вызывает настоящий lazy manager `45A530` через global `75DB84` и
регистрирует объект. `49BBB0` создаёт serializer (20 bytes). Его secondary
table `6ED47C`: writer `49C8D0`, index `49BC90`, reader `49BCE0`.
RTTI initializer `6D4AA0` задаёт serializer ID `047F310F` и parent
`spRenderableSerializer`; target ID — `5AFA1A4F`.

## Уточнения поведения

Source пока завершает ReadPayload только для конечных non-looping parameters
с одной region. Looping initialization вызывает `48D1C0` и уже испускает частицы;
simulation/update/draw, reinitialization/clone и произвольные malformed inputs
остаются открыты. Writer поддерживает также constructor defaults и flags;
наличие writer не означает готовность полного SMO SaveResources.

Source guards проверяют malformed extents, повторную region, нечисловой direction,
неподдержанную looping initialization, zero count и отсутствие ownership cycle.
Профиль `pc-particle-parameters` воспроизводит directed comparisons;
`pc-scene-file-profile` включает whole pickup scene.
