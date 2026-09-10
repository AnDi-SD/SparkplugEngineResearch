# Один PRNG для FunctionEval и Particle

`spFunctionEval::RandomStateForAnalysis` теперь является совместимым alias
существующего `ParticleRandomForAnalysis`. Удалён второй algorithm wrapper
на `std::mt19937`; `spFunctionEval::SharedRandomForAnalysis()` сохраняет свой
единственный singleton. `Seed`/`Next` и explicit analytical states остаются
доступны. Particle sampler принимает этот же state, не создавая второй global
stream и не сбрасывая seed при создании объекта.

Основание — одна original identity: PC `413270`/`4132B0`, 624 слова по
`755658`, index `73FE8C`. Она независимо зафиксирована для
[FunctionEval](native-pc-function-eval.md) и
[Particle sampler](native-pc-particle-sampling.md). Native lazy marker625
вызывает Seed5489; это подтверждённое поведение первого `Next`, не утверждение
о seed реального game startup. Алгоритм, порядок выборок и scalar coefficient
FunctionEval не менялись.

Пять новых assertions в существующем FunctionEval suite перемежают actual
FunctionEval → actual Particle plane sampler → FunctionEval через singleton.
Проверены original seed5489 words1..5 и общий index. После проверки сохранённое
состояние singleton восстанавливается. Root Release run2: **8/8 suites PASS,
19.29s**. Assertions: FunctionEval290, ColorFunction118, MaterialColor235,
UVFunction386, ParticleSerialization46; совместная проверка renderer/cache
также прошла RendererSubmit227, RendererScene574 и FullLoader213.
Первый build остановился только на двух lambda captures нового Renderer test;
root исправил их, после чего run2 завершился. Первый build не считается PASS.

Логи: `particle-loop-init/shared-random-cache-native-build-run2.log` и
`shared-random-cache-ctest.log` под
`local-data/results/tools-core-cycle-20260910-0730/`.
Сохранённая DLL `shared-random-cache-SparkplugViewerNative.dll`:
SHA256 `2B0B7D474DE0EFF16F160DCD59895A4C0ECED6B3169855F49DDA9D08761266EB`.
Новые original RNG runs для alias не понадобились: используются прежние
original scalar/MT и sampler/state comparisons, а свежие frontier runs
не дают дополнительного runtime credit этой замене.

Это удаление дубликата, без реализации looping Particle Init или изменения
loader guard. Отдельная неуспешная original граница описана в
[Particle Init frontier](native-pc-particle-loop-init-frontier-2026-09-10.md).
Fingerprint/validation manifest:
[`tools-core-shared-function-particle-random-2026-09-10.json`](../../research/tools-core-shared-function-particle-random-2026-09-10.json).
