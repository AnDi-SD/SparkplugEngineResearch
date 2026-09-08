# Пакетный C++-эталон для сравнения частиц

CP122, 8 сентября 2026. Оптимизирован frontend
[sampling comparison](native-pc-particle-sampling.md): вместо36 запусков
C++ executable на один region group используется один bounded batch.
Каждая строка создаёт отдельный ParticleSystem и заново задаёт PRNG seed;
движковые алгоритмы и нативные вызовы не изменены.

CLI принимает максимум64 records,128 chars/record,128 positions/case.
Comparer ограничивает response512KiB, ожидает ровно36 JSON rows и сохраняет
5s source-process/30s outer-child caps. Режим `--source-single` оставлен как
измеряемый reference. Native guest по-прежнему исполняет все36 seed/sampler
цепочек с самостоятельным instruction budget и проверкой output sentinels.

[Benchmark](../../research/benchmark_pc_particle_batch.py) выполняет12 свежих
процессов последовательно: два region groups, два режима, три повтора,
с перестановкой порядка режимов. Во всех12 совпали semantic SHA-256,
включающие координаты, полный PRNG state, instruction и random-call counts.

| Group | 36 C++ launches, median | 1 launch, median | Ускорение всей операции |
|---|---:|---:|---:|
| point | 2,401 с | 1,553 с | 1,55× |
| cone | 3,805 с | 3,028 с | 1,26× |

Это дополнительное локальное ускорение, не множитель ко всей скорости проекта.
Batch теперь используется по умолчанию. В итоговом профиле вновь проверяются
все252 случая семи regions. Source regression46 assertions; отдельно проверены
invalid record,65-record overflow и129-position request.

Метаданные замера зафиксированы в
[checkpoint CP122](../../research/native-cycle-checkpoint-2026-09-08-cp122.json).
