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
| `native_specimens.py` | Выбор небольших подходящих ресурсов из уже существующего индекса | SQLite из стандартной библиотеки; локальная база и данные |
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
```

Остальные тесты, включая PS2 arithmetic/spill/division, дополнительно требуют указанных оригинальных файлов и зависимостей. Общий `unittest discover` без подготовленных данных поэтому не является универсальной проверкой свежего клона.
