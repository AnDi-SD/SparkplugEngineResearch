# wxBirdFlyingState

Состояние полёта птицы с selector `20`. Собственные переходы и обработчик
события находятся в [общей реализации](../../../Winx/Code/wxBirdFlyingState.h).
Class ID `18E64F87`, физическая база `wxCharacterState`; native размер `3C`
на PC и PS2. Собственных полей после базы нет. Это компонент состояния;
внешние анимационные и owner-службы требуют явной привязки host.

## Вход, выход и update

Первый вход проверяет базовый once-флаг `1C`, освобождает старый pending,
переписывает key `(key & F03FFFDF) | 00200050`, запускает mode-zero animation,
сохраняет pending и очищает флаг. Затем пишет ноль в action control word
владельца и возвращает false.

Повторный вход выполняет consuming completion query, включая null pending.
Пока animation не завершена, он очищает control word и возвращает false.
При завершении вызывается общий entry: release pending, виртуальный update,
возврат true. Поэтому update может запустить отдельную animation в этом же
вызове, если её флаг `1D` ещё установлен.

Первый выход аналогично использует флаг `1E` и key
`(key & F05FFFD1) | 00400051`, mode zero, очистку control word и false.
При consuming completion вызывается общий exit, который освобождает pending
и возвращает true без виртуального update.

Update выполняет работу только при флаге `1D`: release старого pending,
key `(key & F01FFFDF) | 50`, mode-one animation, запись pending, очистка `1D`.
При mode one предварительный reset completion records не выполняется.
Update сам не очищает owner control word.

Release использует общий выбор stop/fade по predicate владельца. Начало
animation сохраняет interrupt=true и fade selector `0` либо `2`. PC word control
находится через owner `12C`, PS2 — через `138`; portable host отделяет эти layouts.
Собственных permission hooks нет: общие `34` и `38` возвращают true и false.

## Событие и жизненный цикл

Только точное имя `event_takeoff` очищает byte владельца: `26C` на PC,
`278` на PS2. Другая строка оставляет byte прежним. Event и tag заимствованы;
[host](../../../Winx/Analysis/Host/wxBirdFlyingStateHost.h) передаёт имя и
выполняет запись в layout соответствующей платформы.

Constructor меняет только selector базы. Clone создаёт новое состояние,
регистрирует пару и вызывает общий Copy без переноса runtime-полей.
Reset использует базовые once-флаги, pending и movement cache.
Проверка — `WinxBirdFlyingStateTests`.

PS2 выполняет те же integer-переписывания ключа через отдельные byte/halfword
записи. Изменения pending, флагов и control сохраняют тот же порядок.
PS2-проверки ограничены собственными integer prefixes/suffixes вокруг внешних
вызовов; полный связанный PS2 animation/owner graph не объявляется закрытым.
Нулевое имя события и отсутствующий host дают явную portable ошибку.
