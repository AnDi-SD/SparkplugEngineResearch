# CharacterState: формирование ключей и запросы анимации

16 классов,198 успешных исполнений:66 полных PC возвратов при совпадении
текущей анимации,66 PC остановок на настоящем входе запуска при смене,
66 PS2 post-SQ компонентов до входа selector. Пилот0,627s,общий пакет22,655s;
внешние startups не включены. Отказов нет.

Здесь адреса,mask/bits,смещения и raw values шестнадцатеричные; количества
исполнений,индексы таблиц и значения выделенного поля — десятичные.

## Packed key

Каждый метод изменяет word0 переданного аргумента:
`output = (input & mask) | bits`.

| Класс без wx/State | mask | bits | Второй аргумент запуска |
| --- | --- | --- | --- |
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
| TrixAttack | F007FF81 | 1 | 0 |
| WandringNPCWait | F0000000 | 0 | 0 |
| YetiAttack | F01FFF8F | 0 | 0 |

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
