# GamePad: состояния, диапазоны и нормализация PC/PS2

Подтверждены270 уникальных полных вызовов:133 PC и137 PS2. Исследуется
`spDXGamepad` и `spPS2GamePad`, с разными registrations/layout. Настоящие
secondary interface tables находятся вobj14; PS2 исполняет adjustors −14.
PC factory4CC030 задаёт6F30B4/6F3098, PS2 inline factory1F0B80 —
491460/491484. Factory здесь используется как static identity evidence;
устройства, normal startup и производители cached state не исполнялись.

## Сохранённое состояние

| Interface slot | PC | PS2 body после original thunk |
|---|---|---|
| 1: ненулевое значение | 4CB570 | 1F0C80 → 1EFF70 |
| 2: текущее ненулевое, прежнее нулевое | 4CB770 | 1F0C70 → 1EFCB0 |
| 3: значение | 4CBA80 | 1F0C60 → 1EFBD0 |
| 4: разность | 4CBC50 | 1F0C50 → 1EFA10 |
| 5: параметры диапазона | 4CBE60 | 1F0C40 → 1EF8E0 |
| 6: нормализованное значение | 4CBEF0 | 1F0C30 → 1EF7F0 |

PC slots1..4/6 проверяют active byte45; PS2 эти методы не проверяют
active byte. Slot5 PC тоже не зависит от активности. Boolean методы
используют ненулевое значение, включая2/FF; это отличается от строгого
stored==1 в исследованных keyboard/mouse queries. PC Boolean return
квалифицируется по AL, а не произвольным верхним bits EAX.

Все следующие offsets от начала объекта. PC axes111..114 читают words
90/94/A0/A4, previous98/9C/A8/AC. Buttons127..142 читают bytesB0..BF,
previousC0..CF. PC промежуток115..126 и остальные проверенные коды
возвращают0. Slot3 для осей112/114 меняет знак,для buttons возвращает0/1.
Slot4 для этих двух осей даёт previous−current,для остальных current−previous;
buttons вычитаются как исходные unsigned bytes,а не предварительные bool.

PS2 axes111..114 читают words5C/60/54/58, previous6C/70/64/68.
Slot3 тоже меняет знак112/114;slot4 у всех четырёх даёт current−previous.
Buttons читаются по таблице ниже,previous находится на16 bytes дальше.
Slot3 возвращает исходный unsigned byte,не bool.

| PS2 codes | Bytes в том же порядке |
|---|---|
| 115,116,117,118 | 7E,80,7F,81 |
| 127,128,129,130 | 76,78,77,79 |
| 131,132,133,134 | 7A,7C,7B,7D |
| 135,136,137,138 | 75,74,82,83 |

PS2 slot4 **не обрабатывает117/118**, хотя slots1..3 их поддерживают.
139..142 поддерживаются PC,но возвращают0 у проверенных PS2 queries.
Исследовательские имена «current/previous» описывают пары полей и формулу;
их заполнение при hardware polling здесь не заявлено.

## Диапазон и нормализация

PC slot5 задаёт для axes −1000/1000 и float200;для buttons0/1 и float0.
PS2 задаёт axes−127/127 и float изobjB4;для buttons0/255 и float изobjB8,
но поддерживает здесь только115/116,127..134,137/138. PS2 output pointers
проверяются на null по отдельности;проверены также все три null. Для PC
нулевые pointers на поддерживаемом коде не подавались:оригинал их не проверяет.
На неподдерживаемых выбранных codes выходные слова сохраняются.

Оригинальный PC slot6 у axes получает range и slot3 через настоящие virtual
calls. При `abs(v)<=200` результат0,иначе из модуля вычитается200,
значение делится на800 и ограничивается [−1,1]. У buttons возвращается
float от исходного slot3,то есть0/1. Disabled PC не вызывает оба helpers.

PS2 slot6 всегда вызывает оригинальные range/value methods. Пусть `M`
максимум127/255,`f` сохранённая доля,objB4/B8,`d=M*f`. Ненулевой
положительный `v` уменьшается на `d`,отрицательный увеличивается на `d`,
ноль сохраняется;затем деление на `M-d` и ограничение [−1,1]. Отдельного
обнуления внутри dead zone в данном original пути **нет**. При явно
заданном axis fraction0.5,`v=1` даёт примерно−0.984252,`v=−1` —+0.984252.
Это различие сохраняется;исправлять алгоритм по предполагаемому смыслу нельзя.

52 normalized calls покрывают0,малые положительные/отрицательные значения,
границы и saturation,четыре axes,button values0/1/128/255 и active0.
Fixtures используют fractions0.5/0.25. Они **не выдаются за factory defaults**:
static factory записывает приблизительно0.4/0.15. Все callbacks настоящие;
объект целиком неизменен,PS2 восстанавливает S0/S1/SP/RA/F20 и upper64.
PC результат прочитан из x87 physical slot,PS2 — из F0 без подстановки.
Это выбранные finite samples CPU-стенда,не общая bit-exact гарантия EE FPU
на железе. Null/unsupported range в PS2 normalization,NaN,denormals,
деление на ноль и general rounding остаются за пределами подтверждения.

## Проверки, исправление ожидания и учёт

Основной пакет:218 уникальных successes из219 attempts. Первый пилот:
PC прошёл,PS2 axis113 выявил ошибочное ожидание стенда. При чтении окна
переход1EFC7C с delay-slot load1EFC80 был ошибочно принят за fall-through
к соседней оси. Original возвращает word54,как подтверждает правильный CFG.
Исправлен только oracle;failed source/selection/result сохранены. Старый
успешный PC case не повторялся. Пилот2:11 successes,затем пакет206.

При разведке PC tail window был задан с середины инструкции4CB9A9;
он не используется для вывода семантики. Полное дополнительное окно
с точного entry4CB770 сохранено отдельно. Ошибочная выборка prior name
`spDXGamePad` уточнена до настоящего `spDXGamepad` до изменения учёта.

Sparse fixtures с отличающимися соседними ячейками проверяют routing.
Покрыты32-bit negation/subtraction wrap,holes и крайние code values.
Guard4096 bytes для каждого объекта;PS2 original code bytes сохраняются.
Общий CPU wrapper не менялся. 2 assessments +5:PC10→15,PS215→20;
новых C++,полностью закрытых классов и повторного балла base InputDevice нет.

[Контракт и точные источники](../../research/gamepad-cached-state-contracts-2026-09-11.json),
[platform manifest](../../research/native-platform-gamepad-cached-state-2026-09-11.json).
Локальные исходные пробы и runs:
`local-data/results/native-cycle-20260911-0730/gamepad-cached-state/`.
