# PC looping Particle Init: остановленная граница bg_particles

10 сентября 2026. Это original research frontier, без Particle producer
implementation и без успешной whole-file acceptance.

## Подтверждённая зависимость

Reader `49BCE0` вызывает Init `48C340`; render-support initializer `4B97F0`
повторяет pool reset `48BE50` и только при loop!=0 вызывает `48D1C0`.
Этот короткий wrapper ещё раз сбрасывает pool, задаёт accumulator=lifetime,
вызывает producer `48C400(freeCount, clock)` и затем обнуляет accumulator.
Caps query следует после producer. Bounded CFG `48C400..48CD50` содержит
580 reachable instructions, без indirect calls/jumps. Direct callees:
`4132B0`, `41D2D0`, `420350`, `461D70`, `4620C0`, `48C100`, `48EAA0`,
`49E090`, `49E380`. Это статический перечень, не runtime coverage всех ветвей.

Самплеры и PRNG уже восстановлены; producer использует borrowed RenderNode
world transform, направление, скорость, времена и live/free linked state.
Убирать `PrepareNonLoopingForAnalysis` guard без этого состояния нельзя.
Потребитель — cached whole ResourceGraph в Viewer/Corpus typed inspector.

## Единственный реальный вход

`local-data/pc-pristine/Media/Menus/bg_particles.smo`: 17048 bytes, 6 объектов,
SHA256 `8AED1BD2A6A308C5C8AE38EF7AEF5CB96190B4B691DAD181703025AB8D2892A4`.
SQLite file5447: Particle index2/ID3 `stars-001`, offset262, size16784.
Own section16907..17046: loop field7 отсутствует, native default loop1;
times[-1,1.8], default rate100, plane region. File platform1; Texture ID5
`bg_particle` содержит64×64 BGRA32. Минимальность относится к byte_size
индексированных PC-pristine looping SMO, не к native loader acceptance.

Fixture получил отдельный exact64 profile: 16384 pixel bytes, ещё256 bytes
подтверждённого COM row padding, capacity16640, семь mip levels. Старые
tiny/corpus32/tool32 profiles не изменены. Profile guard: 4 assertions PASS.
Оба original child сохранили file1M instructions/8s на вызов,30s child,
128KiB arena,32KiB native allocation cap. PRNG input — явный Seed5489.
Динамические engine bodies, sampler и transform не подменялись.

## Два остановленных запуска

1. `original-run1`: 2.3613s,141868 instructions последнего вызова,
   arena70896. STOP `4677AA`, cursor400: прежний pickup harness регистрировал
   только Texture platform6, а файл требует platform1. Original `6D1880`
   регистрирует `42B660/platform6`; `6D1940` — `42DC30/platform1`.
2. `original-run2`: обе actual registrations выполнены. На cursor16861
   оригинальные `49BCE0→48C340→4B97F0` дошли до `48D1C0`, ещё до own section16907.
   Read-only observer сохранил pool: active0/free100, нулевые clocks,
   constructor parameters, regionTag0, borrowed RenderNode=`CCCCCCCC`.
   Последнее — allocator poison, не native default pointer. Затем observer
   попытался прочесть неинициализированный RenderNode и получил `UcError`.
   **Это hook exception, не instruction/time limit:** выполнено171925
   instructions,2.4675s,arena75664. Старый emulator wrapper добавил к ошибке
   неактуальный текст `instruction/time cap`; он не является доказательством
   исчерпания лимита. Producer ещё не выполнен, teardown после остановки
   не заявлен. Captured pool находится до teardown/подмены состояния.

Root отдельно вызвал текущий shared whole graph: отказ в TextureData ID5
`Invalid bounded derived section`, до Particle. DLL SHA256
`928F59FBB347075791B3385007897809A0B9FB6A3EF4E917C6E1DC32F22D8B93`.
Общая причина двух reader отказов ещё не доказана; они не объединяются в
один вывод о дефекте оригинального producer. Подставной RenderNode,
исправление wire, повторное продолжение остановленного guest и снятие guards
не выполнялись. Этот файл не принят как runtime fixture для порта.

Raw JSON, exact run1/run2 script snapshots, CFG и root shared baseline:
`local-data/results/tools-core-cycle-20260910-0730/particle-loop-init/`.
Tracked harness:
[`probe_pc_particle_loop_init.py`](../../research/probe_pc_particle_loop_init.py).
Manifest:
[`native-pc-particle-loop-init-frontier-2026-09-10.json`](../../research/native-pc-particle-loop-init-frontier-2026-09-10.json).
