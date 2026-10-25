# Статический VU disassembler

`ps2_vu_disassemble.py` обрабатывает уже извлечённые microprogram images из
`ps2_microprogram_inventory.py`. Он вызывает отдельный маленький native decoder,
сохраняет настоящие upper/lower mnemonics и operand names, flags, labels и
ограниченные сведения о прямых ветвлениях. VU-код, emulator, игра и GPU не исполняются.

## Decoder и лицензия

Используется официальный PCSX2 disassembler из
[commit265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34](https://github.com/PCSX2/pcsx2/tree/265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34/pcsx2/DebugTools).
Точные upstream paths, Git blob SHA1 и SHA256 находятся в
[ps2_vu_decoder_source.json](ps2_vu_decoder_source.json).
`DisVUops.h` содержит opcode formatters, `DisVUmicro.h` — dispatch tables;
`DisVU1Micro.cpp` задаёт исходный formatting interface, `VU.h` — register IDs.
`VUops.cpp` используется как отдельный источник проверки interpreter tables.

Эти upstream файлы и `COPYING.GPLv3` сохраняются как внешняя локальная зависимость.
[C++ adapter](ps2_vu_disassembler.cpp) адаптирует formatting glue PCSX2 и явно
помечен `GPL-3.0-or-later`, copyright2002–2026 PCSX2 Dev Team.
Это отдельный ResearchWorkbench executable; его код не включается и не линкуется
в `SparkplugEngine`. Python orchestration не содержит копии opcode tables.
Игровые payload и generated ISA listings не входят в публичный состав.

В старом debugger есть четыре ошибочных cells:
`UPPER_FD_00/01/10/11_TABLE[1]` выбирают `SUBx/y/z/w`, тогда как interpreter
того же commit выбирает `SUBAx/y/z/w` с назначением в ACC.
Adapter явно заменяет только эти четыре function pointers после подключения
неизменённых headers. Protocol identity сообщает `tableRepairs=4`; применённые
cells учитываются в coverage. Synthetic compiled tests проверяют все четыре.

Опечатка formatter `IBGEZ → IBEZ` сохраняется в исходном печатном тексте.
Каждый результат отдельно хранит выбранный leaf symbol, поэтому этот случай
помечается как formatter anomaly и не теряет имя `IBGEZ`. Обнаружение такой
опечатки не даёт основания считать все остальные encodings численно проверенными.

## Подготовка

Получить только перечисленные в manifest файлы из закреплённого commit и
сохранить в отдельный dependency directory. Перед компиляцией сверить manifest
через общий Python helper `verify_sources`; runner повторяет эту проверку до
запуска decoder. Git blob hash дополнительно контролирует исходные окончания строк.
Не подменять отсутствующий dependency файлами другой версии.

В MSVC developer environment собрать adapter отдельно, задав полный путь к
dependency directory и свежий output внутри `local-data`:

```powershell
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"<dependency-directory>" tools/ResearchWorkbench/ps2_vu_disassembler.cpp /Fe:"<fresh-local-decoder.exe>"
```

Проверить `decoder.exe --identity` и записать hash executable вместе с hashes
adapter/dependencies в evidence своего build. Последующая проверка executable
по переданному SHA256 защищает от случайного запуска другого helper; связь
binary с build inputs остаётся частью этого build evidence.

## Decode

Runner принимает manifest исходного извлечения, payload directory, явно
выбранный decoder и его SHA256. Новый output должен находиться строго внутри
`local-data`, summary report — внутри `.private`; существующие пути не перезаписываются.
Каждый input повторно сверяется по длине и SHA256 из extraction manifest.

```powershell
python -B tools/ResearchWorkbench/ps2_vu_disassemble.py --inventory "<extraction-inventory.json>" --payloads "<payload-directory>" --decoder "<decoder.exe>" --decoder-sha256 "<sha256>" --decoder-source-directory "<dependency-directory>" --output "local-data/research-cache/<fresh-isa-output>" --report ".private/evidence/<fresh-report.json>"
```

Один image ограничен2048 instruction pairs, manifest —64 images; decoder
работает последовательно с timeout15s на вызов. CPP stdin protocol содержит
`lowerHex upperHex pcByteHex` и выдаёт одну JSON row на pair. Runtime registers
для форматирования не нужны. Runner проверяет порядок rows, pc и оба raw words.

На image создаются `.isa.txt`, `.isa.json` и `.raw-roundtrip.bin`.
Текст содержит mnemonics и raw prefix `lower32 upper32`; JSON хранит выбранные
opcode symbols отдельно от текста и неизвестных slots. Все результаты
перечитываются и сверяются по hashes. Обратная сборка raw words проверяет
сохранение исходных bytes; это не повторная assembly mnemonics и не numerical oracle.

## Flags, branches и границы

- Upper flags `I/E/M/D/T` сохраняются отдельно. При `I` lower word является
  immediate для I, а не lower instruction; он не увеличивает число decoded lower opcodes.
- Прямые branch targets вычисляются как `index + 1 + sign11(offset)`.
  Внутренние targets получают labels; внешние сохраняются как unwrapped targets.
  Hardware memory wrap не применяется, поскольку VU unit не квалифицирован.
- `JR/JALR` имеют неизвестные indirect targets. Указываются соседний delay slot
  и возможная continuation, но это структурные сведения, не восстановленный reachable CFG.
- `disNULL`, bad op и `.word` не считаются decoded instructions. Recognition
  не доказывает допустимость всех reserved bits или исполнение данного word.
- Сам decoder сохраняет register operand names. Отдельный следующий слой
  строит описанные ниже потенциальные обращения к операндам; pipeline values,
  branch-in-delay behavior, hazards и D/T/M effects им не вычисляются.

Распознанный ISA listing не является семантической C++/HLSL-реконструкцией.
Формат входов VU, вычисления всех ветвей, точная арифметика, VIF channel/unit,
GS/GIF contract и runtime qualification остаются отдельными задачами.

## Архитектурные операнды и прямой CFG

Наш [ps2_vu_dataflow.py](ps2_vu_dataflow.py) читает verified `.isa.json` и
summary предыдущего decode. Контрольные суммы документа и восстановленных
raw words должны совпасть с отчётом. [Reference manifest](ps2_vu_dataflow_source.json)
закрепляет `VUops.cpp`, `VU0microInterp.cpp`, `VU1microInterp.cpp` того же
PCSX2 commit. Эти внешние файлы используются для проверки ISA-фактов и не
копируются в engine. Модуль не вызывает decoder, emulator или GPU.

Для каждой пары сохраняются потенциальные чтения и записи VF по компонентам,
VI, ACC, I/Q и flags. Константные VF00/VI00 и игнорируемые записи выделены
отдельно. Учитываются destination fields, broadcast lanes, фиксированные xyz
у outer products, raw MTIR/MFIR transfers и I-bit: upper читает прежний I,
затем lower immediate записывает новый. Whole-register зависимости MAC/STATUS
намеренно консервативны. Q-result обозначается pending; WAITQ — барьер, а
автоматическое завершение вычисления по тактам не моделируется.

Обращение к памяти хранит base VI, его signed/unsigned interpretation,
qword offset, pre/post increment, выбранные компоненты и ширину. Это
символический адрес: hardware wrapping и alias resolution требуют выбранного
VU unit. XGKICK сохраняет исходный VI, но длина и содержимое GIF packet неизвестны.
Текст `.pseudo.txt` использует непрозрачные операции `VU_*`; он показывает
перенос операндов, не подменяет арифметику VU вычислением Python/CPU.

Direct CFG начинается с явно обозначенного entry0 и сохраняет branch delay
как отдельный context. Conditional outcomes сохраняются оба; BAL/JALR link
не создаёт вымышленный return edge. E останавливает выполнение после
следующей пары; повторный E обновляет countdown. Indirect target, branch в
delay pair, выход за image и M/D/T environment отмечаются unresolved exits.
Первый CFG заканчивается у E-stop. В exit отдельно сохраняется pair-local
TPC/pending branch: установка истёкшего branch предшествует остановке E.
Затем отдельная reference-модель учитывает outer `Execute` обоих pinned
интерпретаторов: branch, начатый в E-delay pair, разрешается перед выходом
из остановленного VU. По известным targets строятся последующие сегменты
со сброшенным E/branch context, до64 сегментов и8×image context states суммарно.
Indirect/outside-image continuation и достижение лимита остаются явными gaps.
Регистры, память и фактическая последовательность MSCNT/MSCAL игры не
переносятся между сегментами этой модели. Не посещённые инструкции не
объявляются мёртвыми; operand footprints есть для всех пар image.

Отдельное `pairPriority` описывает arbitration pinned интерпретаторов.
Совпадение ненулевого номера VF destination upper/lower отбрасывает весь
lower, включая его адресный increment и memory operation. Сравниваются
номера регистров, даже при непересекающихся или пустых lane masks.
Конфликтующие lower reads используют сохранённый целый VF до upper.
Upper CLIP вместе с lower FCSET отбрасывает lower; чтение CLIP в lower
использует прежние flags. При I-bit lower не исполняется, новый I публикуется
после upper. Potential footprints сохраняются отдельно от этих effective
lower effects. Это модель конкретных reference sources, не аппаратная
проверка VU; прочие flags, stalls и scheduled values не выводятся из неё.

```powershell
python -B tools/ResearchWorkbench/ps2_vu_dataflow.py --isa-report "<verified-isa-report.json>" --isa-directory "<isa-output>" --source-directory "<pinned-reference-directory>" --output "local-data/research-cache/<fresh-flow-output>" --report ".private/evidence/<fresh-flow-report.json>"
```

На каждый image создаются `.flow.json` и `.pseudo.txt`; результаты
перечитываются по hash. Пределы —64 images по2048 pairs. Анализ завершается
до создания output directory; существующие пути и одинаковые имена без
учёта регистра отвергаются. Входы и generated program listings остаются
локальными. Scheduled reaching definitions, аппаратная квалификация
dual-issue priority, точные flags/float semantics и полная исполняемая
программа ещё не восстановлены.

## Проверки

Структурные тесты не требуют игры, SDK или native compiler:

```powershell
python -B -m unittest discover -s tools/ResearchWorkbench -p test_ps2_vu_disassemble.py -v
python -B -m unittest discover -s tools/ResearchWorkbench -p test_ps2_vu_dataflow.py -v
```

Два дополнительных теста реально вызывают compiled decoder, если явно задан
`SPARKPLUG_VU_DECODER`. Они проверяют NOP/MOVE, I-immediate, unknown slots,
четыре исправленных ACC cells и raw encoding preservation. Без этой переменной
эти проверки отмечаются skipped, а не как выполненная decoder acceptance.

```powershell
$env:SPARKPLUG_VU_DECODER = '<absolute-decoder.exe>'
python -B -m unittest discover -s tools/ResearchWorkbench -p test_ps2_vu_disassemble.py -v
```
