# Tick: пауза и переходы четырёх триггеров

11 сентября 2026. Изучены собственные v16 ChangeCharacterPlacement,
OpeningGate, PivotingDoor и PushButton. PC вызывает реальные cooldown и
предикаты близости; исполнение останавливается на первом настоящем callback,
если он нужен. PS2 проверяет начальную часть Tick после исключённого SQ.
[Адреса, контракты и источники](../../research/trigger-tick-gate-contracts-2026-09-11.json).

## Общая пауза

Все четыре класса читают кэшированный timer: PC755298 / PS249FC80.
Настоящие getters5968F0/28F020 возвращают byte40. При любом ненулевом byte
Tick возвращаетfalse до cooldown, геометрии и действия; проверены1 и255.
При нулевом byte исполненные пути без callback возвращаютtrue.
Сам timer заимствован как объявленная запись; его холодная фабрика не подменяется.

## Порядок переходов

| Класс | Подтверждённое собственное управление |
|---|---|
| ChangeCharacterPlacement | Сначала cooldown. При его true проверяет v19. Для active=0 и принятой близости вызывает v21; для active!=0 и отвергнутой близости очищает ровно byte active. |
| OpeningGate | При active!=0 вызывает v20; без active сразу возвращаетtrue. Cooldown и v19 здесь не вызываются. |
| PivotingDoor / PushButton | Сначала собственный enabled byte. При enabled=0 возвращаетtrue. Для active=0 проверяет cooldown, затем v19 и приtrue вызывает v21. Для active!=0 сразу проверяет v19, без cooldown; приfalse вызывает v22. После завершения этих ветвей отдельный queued byte вызывает v20. |

Общий active находится в PC126/PS2132. Enabled/queued: Pivot PC168/169,
PS2180/181; Push PC148/149, PS2160/161. Эти рабочие названия описывают
использование, исходные имена полей пока не установлены.

Настоящий PC cooldown590330 проверяет строгое
`uint32(now - previous) > interval`, обновляя previous13C только приtrue.
Вызовы Tick подтвердили границы1099/1100/1101 для previous1000/interval100,
а также wrap FFFFFFFE→1 с interval2/3. При active!=0 дверь и кнопка
вообще не вызывают cooldown: timestamp остаётся прежним.

Реальные v19 используют уже подтверждённую геометрию:
[ChangeCharacterPlacement](native-trigger-distance-predicates-2026-09-11.md),
[PivotingDoor/PushButton](native-trigger-interaction-gates-2026-09-11.md).
Проверены расстояния4/5/6 при threshold25 и противоположное направление
игрока для PushButton. При queued!=0 собственный v19 двери/кнопки отвергает
близость; активный объект сначала доходит до v22. Отдельные inactive пробы
доходят до v20 после cooldown, включая неистёкший интервал.

## Проверки и границы

79 основных исполнений прошли с первого раза. PC61:40 полных возвратов,
6 остановок на v21,6 на v20 и9 на v22 с правильным this. Число реальных
cooldown/v19 вызовов проверено наблюдателями; callback не заменялся успешной
заглушкой и после остановки guest не продолжался. Поэтому результат после
callback и все последующие эффекты не приписываются проверенному префиксу.

PS218 префиксов:11 собственных выходов,3 границы cooldown3AC940,
2 входа v20 OpeningGate и2 входа v19 двери/кнопки. Настоящий getter паузы
исполняется полностью. Ни cooldown result, ни geometry result не подставляются;
полный PS2 Tick с SQ/LQ и дочерними вызовами остаётся открытым.

Пилот16 за1,300s, пакет63 за6,500s внутри процессов. Во всех случаях проверены
36KiB receiver/timer/profile/player/Node records; допустимы только описанные
PC timestamp и active writes. Прочие байты и PS2 code windows неизменны.
Настоящие vtables сохранены, слоты16–22 отдельно сверены с оригиналом.

Восемь updates +5: ChangeCharacterPlacement/PivotingDoor/PushButton PC25→30,
PS220→25; OpeningGate PC20→25/PS215→20. Полностью закрытых классов и новых
C++ реализаций нет. Исходная семантика дальнейших callbacks, нормальная
конструкция зависимостей и cache misses этим блоком не закрыты.

Локальные файлы: `local-data/results/native-cycle-20260911-0730/trigger-tick-gates/`.
В Git: [probe](../../research/probe_trigger_tick_gates.py),
[контракт](../../research/trigger-tick-gate-contracts-2026-09-11.json),
[учёт](../../research/native-platform-trigger-tick-gates-2026-09-11.json).
