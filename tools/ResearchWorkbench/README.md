# ResearchWorkbench

Повторно используемые инструменты анализа executable, ограниченного исполнения и выбора ресурсов из локального индекса. Это наш исследовательский софт, а не восстановленная игровая логика.

| Модуль | Назначение | Зависимости |
| --- | --- | --- |
| `inspect_executable_architecture.py` | Чтение PE/ELF, архитектуры и регистраций | Python, выбранный executable |
| `inspect_serializer_manager.py` | Общие функции разбора executable и идентификации оригинала | Python |
| `capture_native_ranges.py` | Явно заданные диапазоны PC/PS2 | pefile, Capstone, локальные оригинальные файлы |
| `pc_instruction_emulator.py`, `pc_block_emulator.py` | Ограниченный x86 harness и блоки | pefile, Capstone, Unicorn, локальный PC EXE |
| `ps2_scalar_prefix.py` | Ограниченные PS2 scalar-профили | Capstone, pefile, Unicorn, локальный PS2 ELF |
| `ps2_integer_square_accumulator.py`, `ps2_stack_spills.py`, `ps2_unsigned_division.py` | Выделенные вспомогательные операции PS2-профилей | Python; общий transfer-helper где требуется |
| `ps2_integer_multiply.py`, `ps2_raw_transfers.py` | Явные ограниченные MULT/MULT1 и MOVZ/raw FPR seams для packet probes | Python; opt-in через `ps2_scalar_prefix.py`, explicit register/memory state |
| `native_specimens.py` | Выбор небольших подходящих ресурсов из уже существующего индекса | SQLite из стандартной библиотеки; локальная база и данные |
| `pc_shader_inventory.py` | Ограниченный поиск DXSO, source fragments и ссылок в PE | Python; необязательные Capstone и system D3DX |
| `ps2_microprogram_inventory.py` | Проверка и извлечение DMA RET/NOP/MPG upload-образов ELF | Python; явные image-pinned producer bindings |
| `ps2_vu_disassemble.py`, `ps2_vu_disassembler.cpp` | [Статический ISA decode VU](ps2-vu-disassembler.md), flags, labels и encoding readback | Python; отдельный pinned PCSX2 decoder, GPL-3.0-or-later |
| `ps2_vu_dataflow.py` | [Архитектурные операнды VU и прямой CFG с delay contexts](ps2-vu-disassembler.md#архитектурные-операнды-и-прямой-cfg) | Python; verified ISA report и pinned внешние reference sources, без исполнения VU |
| `run.py` | Запуск модуля с явными корнями импортов | Python |

## Запуск

Из корня репозитория:

```powershell
python tools/ResearchWorkbench/inspect_executable_architecture.py --help
python tools/ResearchWorkbench/native_specimens.py --help
python tools/ResearchWorkbench/run.py --list
python tools/ResearchWorkbench/run.py native_workbench.py --help
```

Последний пример доступен только при наличии локальных рабочих экспериментов в `.private/research/`. Launcher ищет имя модуля сначала в публичном workbench, затем в этом закрытом каталоге; запускает дочерний Python из корня репозитория и передаёт оба каталога через `PYTHONPATH`. Аргументы после имени передаются выбранному модулю. Он ничего не скачивает и не запускает эксперименты автоматически.

Повторяемые утилиты остаются публичными. Одноразовые пробы, очереди задач, manifests и результаты хранятся локально. Старые manifests относятся к прежним точным байтам исходников; перемещение файла не превращает их в проверку новой версии.

## Данные и ограничения

Стандартные пути EXE, ELF, базы и необязательных Python-зависимостей заданы в модулях и указывают в существующие локальные каталоги. При обычном клонировании их нет. Анализ не подменяет exeLoader/ОС, а ограниченный harness не является полным эмулятором игры. Лимиты памяти, инструкций и времени сохраняются в соответствующих модулях.

`native_specimens` открывает SQLite только для чтения и выводит выборку в stdout. `select` и `verify` различаются: выбор записи из индекса сам по себе не проверяет весь runtime-путь ресурса.

## Проверки модулей

Не требующие игры проверки:

```powershell
python -B -m unittest discover -s tools/ResearchWorkbench -p test_native_specimens.py
python -B -m unittest discover -s tools/ResearchWorkbench -p test_pc_shader_inventory.py
python -B -m unittest discover -s tools/ResearchWorkbench -p test_ps2_microprogram_inventory.py
```

Остальные тесты, включая PS2 arithmetic/spill/division, дополнительно требуют указанных оригинальных файлов и зависимостей. Общий `unittest discover` без подготовленных данных поэтому не является универсальной проверкой свежего клона.

### Ограниченные EE packet seams

`Ps2ScalarPrefix(..., integer_multiply=True, hi1_lo1=(hi1, lo1))` явно
интерпретирует original MULT/MULT1 words на host. Допускаются только
неотрицательные low32 operands и product не больше `INT32_MAX`. Нужны
existing stack profile и полное начальное `upper64` state; HI1/LO1 тоже
передаются явно. Другие MMI, high-bit/overflow и delay-slot multiply не
допускаются. Default profile эту возможность не включает.

`raw_transfers=True` дополнительно требует `fpr_words` из32 raw words и
`transfer_windows` вида `(address,size,"r"|"rw")`. Это host interpretation
MOVZ и raw LWC1/SWC1; floating arithmetic, FCR и delay-slot transfers
отвергаются по умолчанию. Для normalization probes отдельный
`raw_swc1_beq_delay=True` разрешает только integer BEQ с raw SWC1 delay slot.
Branch condition, target, обе instruction addresses, slot bytes и memory
permissions проверяются до единой записи; два addresses учитываются в cap.
Другие branch/slot combinations и остановка внутри пары отвергаются.
Memory permissions, mapping и alignment проверяются до записи.
MOVZ проверяет всю low64 condition, не только low32. Ни один из этих opt-ins
не является EE hardware/FPU emulator или численной квалификацией VU.

`test_ps2_packet_seams.py` разделяет pure planning/guard checks и пробы с
неизменёнными original instruction words. Отчёт guest содержит каждую
интерпретированную операцию, входной/выходной register state и исходные
границы; алгоритм использует этот explicit research seam, а не незаметную
замену R5900 стандартной инструкцией MIPS. Структура EE multiply и registers
описана в [EE Core User's Manual](https://docs.alexrp.com/mips/ee.pdf),
sections2.11.1/3.1.1/3.1.2; независимый reference — PCSX2 commit
`265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34`, `R5900OpcodeImpl.cpp`/`MMI.cpp`.
Upstream code в эти helpers не копируется.

`scalar_control=True` с явными `scalar_control_windows` включает отдельную
модель integer control pairs. В свежем R4000 profile со stack state она
перехватывает каждый J/JAL/JR/JALR, BEQ/BNE/BEQL/BNEL и BLTZ/BGEZ до backend,
проверяет target, slot, memory permissions и общий instruction cap до записи.
Branch conditions используют low64, register targets — low32; slot видит
новый link, likely-false slot аннулируется. Для slot разрешён небольшой
scalar subset либо уже явные SP-SQ/raw-FPR seams. Неизвестные transfers и
slots отвергаются. Обычный профиль не меняется. Исходные words, trace и
каждое interpreted pair сохраняются; внутренние branch flags backend не
читаются и не исправляются. Это собственная ограниченная ISA-модель для
исследования, а не native EE execution или hardware/exception/timing proof.
Проверки планирования: `python -B -m unittest discover -s
tools/ResearchWorkbench -p test_ps2_scalar_control.py`. Независимые ISA facts:
тот же pinned PCSX2 commit, `pcsx2/Interpreter.cpp`.
Для word arithmetic/shift slots обязательны sign-extended32 operands:
EE instruction manualv6 определяет другой вход как undefined. Такой вход
отвергается, даже если независимый interpreter возвращает конкретное число;
full64 branch conditions проверяются отдельно от word-slot arithmetic.
Отдельные slot cases `DSRA32` и `SLTIU` используют low64 без word narrowing:
арифметический сдвиг на`32+sa` и unsigned comparison с sign-extended16
immediate соответственно. Upper64 остаётся в явном shadow state. Эти
операции описаны в [EE instruction manualv6](https://docs.alexrp.com/mips/ee_insns.pdf),
pages58/108; другие неизвестные slots по-прежнему отвергаются.

## Статический каталог PC shader sources

`pc_shader_inventory.py --help` описывает входные PE и обязательный свежий
`--output` внутри `local-data/research-cache`. Образы читаются без исполнения;
одинаковое содержимое разных входов учитывается через aliases. Существующий
output не перезаписывается. Извлечённые raw bytes и строки остаются локальными.

Version-сигнатура, ограниченно корректный DXSO stream, SDK disassembly и
доказанный игровой producer — разные уровни. Сканер сохраняет причину отказа,
границы секции и xref-кандидаты; линейное окно или E8 encoding не объявляет
полный CFG. Известная таблица параметров интерпретируется только для
явно установленных image identities. Отрицательный raw scan не закрывает
упакованные или временно создаваемые программы.

## Статический каталог VU-программ

`ps2_microprogram_inventory.py --help` описывает явные ELF, SHA256, bindings,
payload output и metadata report. Полный контейнер в узкой грамматике
DMA RET + NOP/MPG сначала считается кандидатом. Извлечение требует также
литеральной MIPS-ссылки и привязанных к hash оригинала проверенных окон producer
и upload-кода. Неизвестная VIF-команда, разрыв или перекрытие MPG-адресов,
выход за DMA/QWC или file-backed section отвергаются.

Байты контейнера, собранная VU-программа и listing пар lower/upper words
пишутся только в свежий каталог `local-data`; metadata — в локальный evidence.
Каждый MPG-фрагмент сохраняет исходные VA/file offset, длину и SHA256.
Обратная подстановка фрагментов и повторное чтение проверяют точные байты.
Listing не является VU ISA disassembly. Метод не доказывает исполняемость,
назначение вычислений, конкретный VU unit, все upload-маршруты или GS state.
