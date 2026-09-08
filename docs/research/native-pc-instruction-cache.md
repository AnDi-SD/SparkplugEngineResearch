# CP117: ускорение свежего PC guest

8 сентября 2026. Три последовательных прохода по трём original/source операциям
подтвердили ускорение всего дочернего процесса в1,86–2,46 раза. В каждом проходе
менялся порядок вариантов. Все27 операций завершились, результаты и числа
оригинальных инструкций совпали. Это ускорение исследовательского стенда;
class scores и покрытие поведения от него не повышаются.

| Операция | Полный PE, bytes cache | Fast PE, bytes cache | Fast PE, page cache | Выигрыш |
|---|---:|---:|---:|---:|
| Whole g_crystal.smo,26 объектов | 8,713 с | 7,492 с | 4,533 с | 1,92× |
| Whole gem.smo и4 UV updates | 6,163 с | 4,941 с | 3,316 с | 1,86× |
| Common P8 3×5 → DXTexture | 5,034 с | 3,701 с | 2,045 с | 2,46× |

Здесь медианы полного fresh-process времени, включая запуск Python,
подготовку PE/fixtures, C++ сравнение, original load/conversion и cleanup.
Последовательность original whole load сохраняет856194/395857/447211
инструкций соответственно. Peak working set дочернего Python по Windows API
не превысил100,97 МиБ; новый page-вариант —100,19 МиБ. Это максимум одного
процесса, без C++ child, управляющего процесса и других приложений.

## Два устранённых расхода

`pefile.PE(..., fast_load=True)` пропускает ненужные полную обработку directories
и подсчёт всех байтов файла. Используются только PE headers и sections;
Windows loader/imports всё равно не эмулируются. Отдельный замер mapping:
1,3298→0,0585 с. Полный образ23110717 байт совпал по SHA-256:
`6916758669374E945BECFBD292B960235CBA37339752AF284FFB121E2DFB97B1`.
Base `00400000`, SizeOfImage23113728 также совпали. Перед parse по-прежнему
проверяется SHA-256 исходного EXE; вывод относится к закреплённому PC binary.

Ранее callback каждой инструкции читал её bytes через Unicorn API, даже при
попадании в `(address, bytes)` decode cache. Профилировщик gem выявил483143
memory reads и2,236 с внутри `mem_read`. Новый cache хранит `(length, mnemonic)`
по адресу, а также множество адресов на каждой4096-byte странице. Если запись
затрагивает страницу, все её cached instructions удаляются. Инструкция на
границе включается в оба множества. Проверка length остаётся при каждом hit.

Guest writes отслеживаются `UC_HOOK_MEM_WRITE` внутри PE. Host fixture writes
через `mu.mem_write` обёрнуты отдельно: Unicorn API не вызывает guest-write
hook. `mem_unmap` также отменяет cache. При host изменении cached page
сбрасывается Unicorn translation cache: без этого замена1-byte NOP на5-byte
MOV сохраняла старую длину, и guard останавливал исполнение. Регрессия сохранена.
Обычные native self-modifying bridges исполняются как раньше; engine functions
для ускорения не заменяются. Bytes cache оставлен явным reference mode.

## Проверки и воспроизведение

Новые guard regressions охватывают cached host patch в HLT, native guest
self-modification в HLT, изменение второй страницы straddling instruction,
избирательную invalidation, unmap и изменение длины. Прежние проверки NULL,
interrupt/privileged operations, instruction/process caps и FS fixture сохранены.
Guard suite17/17, общий Python suite102/102. Native execution caps прежние:
micro100000/2 с, file1000000/8 с, child30 с; arena64/128 КиБ.

После включения default cache прошли whole-scene9/9, common raw101/101,
compressed missing-mip15/15 и SAN roundtrip2/2 profile children. Повторный
замер прежнего10-case raw missing-mip профиля дал12,436 с при1 worker и
5,328 с при4 workers; sampled aggregate working set118,34/332,65 МиБ.
CP110 до этих изменений измерил29,79/10,44 с тем же инструментом: для
четырёх workers теперь примерно в1,96 раза быстрее. Разные моменты замера
состояния машины не являются контролируемым сравнением всей работы.

```powershell
python research/benchmark_pc_instruction_cache.py --repeats 3
python research/test_pc_instruction_emulator.py
```

Benchmark запускает строго по одному новому процессу, требует точного
original/source совпадения и сохраняет raw captures только в ignored local-data.
`baseline`/`fast-pe`/`page` меняют только подготовку и cache harness, никогда
пределы guest или native алгоритмы. Один benchmark владеет общими capture names;
не запускать его одновременно с такими же scene/P8 fixtures.

[Манифест CP117](../../research/native-cycle-checkpoint-2026-09-08-cp117.json)
содержит27 metadata rows, fingerprints и измерения. Четыре независимых workers
из [CP110](native-parallel-profiles.md) совместимы с новым cache; выигрыши
разных замеров нельзя автоматически перемножать для всего исследования.
