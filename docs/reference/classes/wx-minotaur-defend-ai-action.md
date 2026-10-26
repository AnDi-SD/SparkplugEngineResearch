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
Собственные тела state `0` и `1` пока не восстановлены.
`UpdateGateForAnalysis` возвращает для них явные `UnqualifiedState0/1`
до owned-эффектов. Bool virtual адаптер возвращает false с тем же сохранённым
результатом и диагностическим наблюдателем; это **наш отказ адаптера**, а не
найденный native false. Host не может объявить эти тела успешно исполненными.

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
точный целочисленный профиль. Полная EE arithmetic, owned state bodies,
selector delivery, normal scene startup и вся игровая AI-система не закрыты.
PS2 Copy квалифицируется отдельным собственным суффиксом с явным `V0=1`;
это не полный PS2 Copy или constructor/lifetime.

`WinxMinotaurDefendAIActionTests` проверяет порядок Entry, реальную общую
perception/RNG логику, cached command, signed-wrap Notify, повторный PS2
timer read, ограниченный gate, отказ unknown stages, четыре Copy-поля,
заимствованное владение, clone и RTTI.
