# `wxMinotaurDefendAIAction`

`wxMinotaurDefendAIAction` (`2F567161`) имеет физическую и регистрационную
базу `wxAIAction`. Native allocation — PC `3B8`, PS2 `3BC`.
[Общий исходник](../../../Winx/Code/wxMinotaurDefendAIAction.cpp) отделён от
[PC ABI](../../../Winx/Analysis/PC/wxMinotaurDefendAIActionAbi.h) и
[PS2 ABI](../../../Winx/Analysis/PS2/wxMinotaurDefendAIActionAbi.h).

| Поле | PC | PS2 | Constructor |
| --- | --- | --- | --- |
| Заимствованная цель | `3A8` | `3AC` | null |
| State | `3AC` | `3B0` | 1 |
| Deadline | `3B0` | `3B4` | 0 |
| Cached command | `3B4` | `3B8` | null |

## Entry, Query и Exit

Entry (`5B8A40/25FCE0`) выполняет общий
[поиск ближайшей цели](wx-perception.md), записывает цель и state `0`,
захватывает current time, получает одно число `r` настоящего общего RNG
и записывает `deadline = uint32(now + 3000 + r % 1501)`. Затем перечитывает
owner и кэширует его Character command. Если цель отсутствует, selector
получает key `1`, parameter `0`. RNG и кэширование выполняются до этого
запроса даже при отсутствии цели.

Query (`5B4970/25FAF0`) снова ищет ближайшую цель, независимо от permission
byte, и при её отсутствии запрашивает key `1`, parameter `0`.
Cached target при Query не заменяется. Exit (`5B8920/25FCC0`) обнуляет
байты `1D` и `1F` сохранённого command, затем cached target. State, deadline
и cached command сохраняются. Command для Exit не получается заново из owner.

Source использует общую реализацию `wxPerception` и уже существующий
`spFunctionEval::RandomStateForAnalysis::Next`. Копии RNG-алгоритма нет.
Служба времени `755298/49FC80` — `wxGameTimer`, чей constructor строит
`spTaskTimer` prefix. Адаптер предоставляет заимствованный `spTaskTimer&`,
из которого читается `currentMilliseconds`; полный `wxGameTimer` и его
ленивое создание не подменяются другой timer class.

## Notify и ограниченный update gate

Notify (`5B8AD0/25FBF0`) обрабатывает `2717` следующим образом:
`remaining = int32(uint32(deadline - now))`; если `remaining < 3000`,
deadline продлевается до `uint32(now + 3000)`. Равенство `3000` не продлевает
срок. Это знаковое сравнение после 32-битного wrap, поэтому большое unsigned
remaining может интерпретироваться как отрицательное.

PC использует единственное захваченное now. PS2 при принятом продлении
повторно читает global timer и его current field. `27A4` поглощается без
таймера; остальные сообщения передаются общей базе `wxAIAction`.

Update gate (`5B8D80/25FDB0`) сначала сравнивает time и deadline как unsigned:
при `now > deadline` запрашивается selector key `2`, parameter `0`.
При неистёкшем deadline неизвестный state заменяется на `1`.
`UpdateGateForAnalysis` ограничивается этим решением и возвращает для state
`0/1` значения `UnqualifiedState0/1` до собственных эффектов.

## Собственные PC state `0/1`

Явный профиль `PcFiniteGeometry` дополнительно выполняет собственные PC тела
`5B8C30/5B8CE0` и facing-предикат `5B8BB0`. По умолчанию остаётся `GateOnly`;
PC профиль не включает эти операции для PS2.

Оба тела сначала устанавливают byte `1F` cached command. State `0` получает
направление от cached world position owner к cached world position target,
записывает motion `04 = 0.15f`, затем heading `08` и при достаточном совпадении
направления меняет state на `1`. State `1` записывает motion `04 = 0` до чтения
геометрии, сохраняет heading и меняет state на `0`, когда совпадение потеряно.
Cached target и command остаются заимствованными; нового поиска цели нет.

Facing использует **локальную** третью строку orientation `spNode`
(`58/5C/60`), а позиции берутся из world cache `74/78/7C`. Это разные
системы координат; подмена локальной строки cached world orientation меняет
поведение при повёрнутом родителе. Y в этих операциях игнорируется.
Горизонтальный вектор нормализуется общим `41D2D0`; yaw равен `acos(z)` с
отрицательным знаком при `x < 0`. Нулевой вектор даёт yaw `pi/2`, а `-0`
в X не выбирает отрицательную ветвь. Facing проверяет строгое
`shortestAbsoluteAngle < 0.1f`; равенство порогу не принимается.

[Общая PC математическая операция](../../../Winx/Analysis/PC/wxPlanarAngleMath.h)
переиспользует существующий конечный адаптер нормализации и host `acos` для
внешнего `MSVCR71!_CIacos`. Это явно ограниченная численная замена, а не
побитовая реализация импортированной CRT или произвольной x87 среды.
Профиль требует конечные X/Z в пределах `[-1024,1024]`, готовые Node caches,
валидные borrowed ссылки и стабильные геометрические bindings. Собственные
данные читаются из настоящего общего `spNode`, а не результата callback,
который объявляет всё тело успешным.

Выход за численный domain возвращает `UnqualifiedGeometry`. Такой наш отказ
может следовать за уже выполненными начальными command writes. Виртуальный
bool адаптер возвращает false для неразрешённого state/domain и сохраняет
диагностику; native PC update для квалифицированных тел возвращает true.

## Copy и границы

Copy выполняет общий base Copy, затем переносит ровно четыре собственных
слова в порядке таблицы. Target и command копируются как raw borrowed refs;
в собственных ctor/dtor/Copy нет их AddRef или Delete. Destructor передаёт
управление общей базе, не вызывая Exit. Clone сохраняет эти собственные
ссылки через Copy, но не переносит базовый runtime owner.

Внешние owner/Character, command, selector, timer prefix и фактический
общий RNG state предоставляются обязательным
[host-контрактом](../../../Winx/Analysis/Host/wxMinotaurDefendAIActionHost.h).
PC использует конечный nearest64 профиль Perception, PS2 — ограниченный
точный целочисленный профиль. Полная EE arithmetic, PS2 owned state bodies,
selector delivery, normal scene startup и вся игровая AI-система не закрыты.
PS2 Copy квалифицируется отдельным собственным суффиксом с явным `V0=1`;
это не полный PS2 Copy или constructor/lifetime.

`WinxMinotaurDefendAIActionTests` проверяет порядок Entry, реальную общую
perception/RNG логику, cached command, signed-wrap Notify, повторный PS2
timer read, ограниченный gate, отказ unknown stages, четыре Copy-поля,
заимствованное владение, clone, RTTI, PC finite geometry и строгую границу
facing с отдельным отказом для PS2/неподдерживаемой геометрии.
