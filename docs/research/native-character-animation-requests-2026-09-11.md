# CharacterState: формирование ключей и запросы анимации

16 классов,198 успешных исполнений:66 полных PC возвратов при совпадении
текущей анимации,66 PC остановок на настоящем входе запуска при смене,
66 PS2 post-SQ компонентов до входа selector. Пилот0,627s,общий пакет22,655s;
внешние startups не включены. Отказов нет.

PC вызывает настоящий [wxAnimationManager lookup](native-character-animation-selection-2026-09-11.md).
В явно заданной таблице есть default key0 сvalue13572468. При stage=same
state24 имеет это значение,поэтому ветка повторного запуска не выполняется.
При stage=changed исходный state24=0,selector возвращает другое значение,
и проверка доходит до настоящего helper512EA0 с точными аргументами.
Исходный stop helper512E40,если его вызывает данный класс,возвращается сам
при нулевом state24. Ни одна игровая функция не заменяет результат другой.

Здесь адреса,mask/bits,смещения и raw values шестнадцатеричные; количества
исполнений,индексы таблиц и значения выделенного поля — десятичные.

## Packed key

Каждый метод изменяет word0 переданного аргумента:
`output = (input & mask) | bits`.

| Класс без wx/State | mask | bits | Второй аргумент запуска |
|---|---|---|---|
| Action | F007FF8F | 0 | 1 |
| DateIdle | FF800059 | 59 | 1 |
| DateTalking | FF800E09 | E09 | 1 |
| DroidInactive | F0078003 | 3 | 1 |
| Flying | F007FFF1 | 1 | 1 |
| IceWormAttack | FF987FDF | 50 | 0 при `((output>>7)&FF)` равном8/10,иначе1 |
| LadderSlide | F080002F | 800020 | 1 |
| MikaelOpenGate | F1000000 | 1000000 | 0 |
| MikaelWaiting | F0800000 | 800000 | 1 |
| Missile | F01FFFF1 | 1 | 0 |
| OpenGate | FF878A80 | A80 | 0 |
| Reading | F0078900 | 900 | 1 |
| ShadowBeastDefense | F0000180 | 180 | 1 |
| TrixAttack | F007FF81 | 1 | 0 |
| WandringNPCWait | F0000000 | 0 | 0 |
| YetiAttack | F01FFF8F | 0 | 0 |

Все68 indexed tables не загружаются из игры этим экспериментом: задана
borrowed структура только для проверяемого поиска,её lookup-читаемые поля
соответствуют предыдущему контракту. Подготовка owned container и его полные
инварианты не заявляются. Для каждого класса проверены input0/FFFFFFFF/
12345678/80000000,индексы0/67; у IceWorm дополнительно400/500,которые
различают ветви параметра запуска. Неизменяемые bytes всех6000h bytes
borrowed области также проверены.

Перед selector PC59A5E0 зафиксированы this=manager и arguments(output,index).
Index взят из character128,при state14→character; manager — явно заданный
ненулевой global765ADC. После реального lookup получен value13572468.
При смене анимации PC512EA0 получает this=state и три arguments:
`(returnedValue,флаг из таблицы,1)`. Имена исходных параметров и полная
семантика playback этим блоком не устанавливаются.

## Дополнительные записи и границы

При полном PC same-path Action иOpenGate обнуляют move word4 через
state14→character12C. Остальные tested same-path сохраняют данные state.
При changed-path MikaelOpenGate иWandringNPCWait перед запуском ставят
state byte3C=1. State24 сохраняет исходное значение до остановки512EA0;
запись нового значения после helper здесь не исполнена.

Nonzero old animation→different animation,реальная остановка старого playback,
сам запуск и его эффекты,normal manager startup/loader остаются открыты.
Доказанное совпадение current value не переносится на полный lifecycle класса.

PS2 использует собственные LBU/LHU/AND/OR/SB/SH последовательности и передаёт
такой же packed output. Начало — после последнего SQ пролога; исходные
нулевые/immediate вычисления регистров до него отдельно разобраны и сохранены
как `ps2InitialConstants` с точными инструкциями. Исключены только ABI stack
saves,полный пролог не объявляется исполненным. Все16 прочитанных прологов
в этой выборке используют для начальных temporary registers только эти
константные операции,а не неизвестную игровую функцию.

PS2 завершается на настоящем selector273D10 сA0=manager,A1=output,A2=index.
State14→character134 поставляетindex,global49FDA0 при GP4A4170 — manager.
Вложенный selector сSQ-прологом в этой транзакции не выполнялся; отдельное
доказательство selector не подменяет полный PS2 playback path.

Evidence: `local-data/results/native-cycle-20260911-0730/character-animation-requests/`.
Оценка каждого из16 уже известных классов повышена на5 независимоPC/PS2.
Это новый контракт v12,без повторного credit за factory,общий lookup или
раньше проверенные permission/event методы. Новых C++ реализаций и полностью
закрытых классов нет.
