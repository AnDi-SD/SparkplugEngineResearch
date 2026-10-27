# `wxHurtAIAction`

`wxHurtAIAction` (`4D8057F6`) физически и регистрационно наследует
[`wxAIAction`](wx-ai-action.md). Размер native объекта — PC `3AC`, PS2 `3B0`.
Собственный latch — один байт `3A8/3AC`; constructor обнуляет его, не обещая
инициализации трёх завершающих padding bytes. [Общий исходник](../../../Winx/Code/wxHurtAIAction.cpp)
отделён от [PC ABI](../../../Winx/Analysis/PC/wxHurtAIActionAbi.h) и
[PS2 ABI](../../../Winx/Analysis/PS2/wxHurtAIActionAbi.h).

## Вход, выход и latch

Entry (`5A9ED0/2509D0`) получает Character command через owner. Null command
завершает метод, сохраняя latch. Иначе Entry устанавливает byte `5C = 1`,
записывает byte `60 = (argument != 0)`, word `04 = 0` и только затем очищает
latch. Command перечитывается перед вторым и третьим store. Аргумент native
Entry — 32-битное слово; `EnterForAnalysis` сохраняет его явно, а общий
безаргументный `vfunc_30` адаптер выбирает `0`.

Exit (`5A9F30/2509B0`) очищает только command byte `5C`. Notify и Query —
буквальные no-op, без передачи уведомления текущему дочернему action.

Update (`5A9E60/250A30`) захватывает начальный latch, затем читает Character
state. При latch `0` и state `A` он вызывает Reset command и устанавливает
latch `1`. При latch `0` и другом state или ненулевом latch и state `A`
Update возвращает true без остальных эффектов. Ненулевой latch и выход из
state `A` вызывают собственный range helper, затем очищают latch и запрашивают
selector key `2`, parameter `0`.

Reset command — отдельный foreign receiver с доказанными таблицами
`703090/494710` и методами `5992E0/3730E0`; имя его native класса пока неизвестно.
Host предоставляет этот конкретный внешний контракт, а не callback,
который заменяет весь owned Update.

## Собственный range helper PC

Метод `5AA070` находится в **восемнадцатом** PC vtable slot `44`; каталог
первых семнадцати slots его не перечисляет. Он читает **локальные** позиции
Node owner и player (`20/24/28`), а не cached world positions. Player приходит
из уже готового профиля `765AD4`, поля `2B4` и Node по `24`.
Этот data binding не приписывает профилю неподтверждённый native класс.

Расстояние сохраняется как float32. Если оно строго больше Perception word
`3C`, в это поле записывается `(distance + 100)^2`, затем вызывается настоящий
PC Perception Refresh. Сравнение расстояния с полем, куда сохраняется квадрат,
сохранено буквально; исправление размерностей изменило бы оригинальную логику.
Perception перечитывается для comparison, записи и Refresh.

Явный `PcSmallIntegerRange` профиль использует общий `spNode`,
[`wxPerception`](wx-perception.md) и настоящий `wxGameTimer`. Координаты
ограничены целыми значениями `[-1024,1024]`; range должен быть конечным и
неотрицательным. Холодные профиль/Timer и расширение Scene scan не подменяются
успешными заглушками. При paused GameTimer настоящий Perception Refresh
возвращает false; Hurt Update игнорирует этот результат и продолжает выход.
При null owner у Perception разрешён его ограниченный завершающийся Refresh.

Если Refresh доходит до неподтверждённого owner-bound Scene scan, source
сохраняет уже выполненные range/Perception prefix эффекты и возвращает явный
`UnqualifiedPerceptionRefresh` до очистки latch и selector. По умолчанию
`IntegerHooksOnly` не включает range helper; этот PC профиль не включает
PS2 floating-point helper `250810`. Аналитический bool false в этих отказах
не является найденным native false.

## Copy и границы

Copy (`5AA030/250920`) выполняет общий base Copy и завершение, **не копируя
собственный latch**. Destination сохраняет свой байт, а Clone сохраняет
constructor latch `0`. Cached native storage, полный inherited pathfinding,
Game startup, selector delivery, активный Scene scan Perception и PS2 range
arithmetic остаются отдельными открытыми областями.

`WinxHurtAIActionTests` проверяет command stores/повторные чтения, null Entry,
latch handshake, Copy/Clone/RTTI, real shared Timer/Perception consumer,
range dimensional quirk и явные численные/платформенные границы.
