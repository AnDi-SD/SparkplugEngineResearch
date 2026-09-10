# Ускорение PC guest: проверка блоков и разблокирование Bloom

Добавлен явный `PcBlocks` для длинных original вызовов. CPU по-прежнему
исполняет исходные x86 bytes в Unicorn; Python проверяет и учитывает basic
blocks вместо callback на каждую инструкцию. Игровой код, protected callee
и алгоритмы конструкторов не заменяются. Прежний `PcInstructions` остаётся
для точной инструкции, tail и instruction coverage.

## Измерение и предел доказательства

Fresh guests, одинаковые original inputs, fixtures и native instruction caps.
Время ниже — внутри original calls, без запуска Python и записи отчётов.

| Случай | Instruction trace | Block trace | Выигрыш |
|---|---:|---:|---:|
| CharacterStateMachine, пять lifecycle операций | 0,359 с | 0,159 с | 2,26× |
| Bloom factory, остановка на 1M | 2,997 с | 0,388 с | 7,72× |
| Bloom factory, остановка на 6M | 15,610 с | 1,466 с | 10,64× |
| AIAction cold path до missing scene | 6,159 с | 0,611 с | 10,08× |

Во всех четырёх парах совпали semantic SHA: сохранённые GPR/x87 состояния,
хеши mapped PE image, heap arena и stack, allocation requests/frees,
clone-map observations и timer refreshes. Совпали также возвраты/остановки
и адреса EIP. Это направленная проверка режима на данных путях; она не
объявляет весь реверс или каждый guest в десять раз быстрее.

Артефакты: `local-data/results/native-cycle-20260910-1900/block-trace/`.
`comparison-summary.json` содержит четыре сравнения; отдельные reports —
все checkpoints. Точные версии источников сравнения сохранены в
`benchmark-sources/`, предыдущая версия emulator — в `pc_instruction_emulator-before.py`.

## Что сохраняется и что меняется

- Pristine SHA, guest-only память, отсутствие host API forwarding, heap bounds,
  unmapped-memory/interrupt guards, 30-секундный внешний процесс сохраняются.
- Каждый block декодируется до исполнения; invalid/privileged block отвергается
  целиком у его входа. При такой ошибке остановка может быть раньше, чем в
  instruction trace. Это явно помеченная граница, не успешный callee.
- Host patch и guest self-modification сбрасывают validation cache, в том числе
  когда инструкция пересекает страницу. Две стороны этого случая проверены.
- Fixture callbacks и requested boundaries сохраняют точные адресные hooks.
  Набор seams фиксирован на время одного вызова, синхронизируется перед следующим.
- `visits`, `tail`, `factoryVisits` в новом режиме отражают **входы в блоки**.
  Lifecycle report пишет `blocks`/`lastBlockCount` и `tracer.visitUnit`; это
  не instruction count и не доказательство посещения каждого адреса блока.
- Общая проверка mnemonic теперь учитывает REP/REPNE prefixes: `rep insb`
  и `rep outsb` также отвергаются. Это уточнение исследовательского стенда;
  I/O в host и прежде не перенаправлялся.

Проверки: прежние 18 guard/cache tests прошли за 2,054 с; первые девять
block tests — за 4,020 с. После добавления extended profile десять block
tests прошли за 3,206 с. Проверены exact boundary внутри блока, изменение
seams между вызовами, одинаковое состояние на native cap, host/guest patch,
изменение внутри текущего блока, cross-page invalidation и запрещённые инструкции.

## Применение к Bloom

На основании измерения введён профиль `protected-block`: максимум 60M native
instructions, **те же 24 с на вызов и 30 с на guest process**. Instruction
tracer этот профиль отвергает. Это явный fresh-guest профиль после сохранённого
6M cap; остановленный guest не продолжался, память не расширялась.

`pc-wxBloomStateMachine-run5.json` в `character-state-machine/`:

- original factory `004044C0`, RTTI `00528270`, clone `0040ABF0`, обе операции
  deleting entry `005283C0` вернулись;
- весь probe — 2,340 с, размер объекта и клона PC `2CC` (716) байт;
  независимо ранее PS2 factory выделял `2F0` (752), delta36;
- arena32192 байта, 140 allocation requests, 132 frees; оставшиеся восемь
  выделений не объявляются утечкой без установления их глобальных владельцев;
- factory наблюдался в128115 block entries. Точное число его инструкций
  этим отчётом не измерено; 60M — верхний предел, а не потраченный объём.

Таким образом, семейство теперь имеет 36 PC factory/RTTI/default clone и
35 полных PC lifetimes, 178 class operations. Goop machine+24 остаётся
отдельной задачей normal binding. Предыдущие failed Bloom reports и исходный
construction dossier сохраняются как история наблюдений.

Для Bloom добавляется первый PC scouted score20. PS2 assessment15 сохраняется:
ускорение PC стенда не является новым доказательством PS2 исполнения.
Active переходы и содержимое изменённого runtime clone ещё не закрыты.
