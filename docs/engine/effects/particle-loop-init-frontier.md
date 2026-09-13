# PC looping Particle Init: остановленная граница bg_particles

## Подтверждённая зависимость

Reader `49BCE0` вызывает Init `48C340`; render-support initializer `4B97F0`
повторяет pool reset `48BE50` и только при loop!=0 вызывает `48D1C0`.
Этот короткий wrapper ещё раз сбрасывает pool, задаёт accumulator=lifetime,
вызывает producer `48C400(freeCount, clock)` и затем обнуляет accumulator.
Caps query следует после producer. Bounded CFG `48C400..48CD50` содержит
580 reachable instructions, без indirect calls/jumps. Direct callees:
`4132B0`, `41D2D0`, `420350`, `461D70`, `4620C0`, `48C100`, `48EAA0`,
`49E090`, `49E380`. Это статический перечень, не runtime coverage всех ветвей.

Убирать `PrepareNonLoopingForAnalysis` guard без этого состояния нельзя. Потребитель — cached whole ResourceGraph в типизированном инспекторе Viewer.
