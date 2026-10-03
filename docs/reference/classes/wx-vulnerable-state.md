# wxVulnerableState

Class ID `1B815375`, selector `28`, физическая база `wxCharacterState`.
Оба constructor вызывают constructor базы; native размер — `40` байт.
Собственные byte `3C/3D/3E` получают `0/0/1`. Их исходные имена неизвестны.
Общая [реализация](../../../Winx/Code/wxVulnerableState.h) хранит поля состояния
отдельно от [адаптера внешних объектов](../../../Winx/Analysis/Host/wxVulnerableStateHost.h).

## Вход и update

Первый вход переписывает key как `(key & F120800F) | 01208000`, выполняет
lookup, release прежнего pending и очередь нового с mode=false, interrupt=true.
Затем сохраняет handle, обнуляет `3C` и once-флаг `1C`. До завершения анимации
вход очищает action control word `4` и возвращает false. Повторный вход
проверяет consuming completion; при true вызывает общий вход, который
освобождает pending и вызывает виртуальный update этого состояния.

Общий собственный entry содержит отдельную ветвь selector `33`: при нулевом
`3D` он сразу вызывает базовый вход. Constructor данного класса задаёт `28`;
аналитическое изменение selector не приписывается игре как обычный вызов.

Update всегда задаёт `3C = 1`. Key сначала получает
`(key & FF80800F) | 8000`, затем `(key & F1FFFFFF) | 01800000` при selector
`33`, иначе `(key & F17FFFFF) | 01000000`. При том же handle он сохраняет
pending и не вызывает playback. При изменении выполняет release и новую
очередь: mode=true только при selector `28`, interrupt=true всегда.

Permission `34` при selector `28` возвращает значение byte `3C` как bool,
при прочих selector возвращает true для null pending, иначе consuming
completion. Hook `38` унаследован и возвращает false.

## Выход и внешние поля

Если `3E` ненулевой и control byte `5D` владельца нулевой, первый выход
выполняет изменения внешнего manager, задаёт key
`(key & F140800F) | 01408000`, делает lookup, release и очередь mode=false,
interrupt=true, сохраняет pending и очищает once-флаг `1E`. Пока consuming
completion не вернул true, очищает action control word и возвращает false.
При завершении вызывает общий exit с release и true, без повторных изменений
manager. Если `3E` нулевой либо byte `5D` ненулевой, выполняет изменения
manager и сразу общий exit.

Операция внешних полей получает текущий lazy manager и очищает его byte `50`.
При ненулевом byte `2C`, ненулевом borrowed owner `20` и ненулевом owner byte
`150` PC / `15C` PS2 очищает owner byte `144` PC / `150` PS2. Условные ветви
выполняет само состояние. PC читает manager byte `2C` до записи `50`; PS2
пишет `50` до чтения `2C`. Platform profile сохраняет этот порядок.

## События и жизненный цикл

Event сначала передаёт tag внешнему diagnostic logger с форматом
`Animation tag : %s`, затем проверяет owner entity kind `14C` PC / `158` PS2.
Только при kind `24` точный tag `event_knee_begin` обнуляет owner byte `224`
PC / `230` PS2, а `event_knee_end` задаёт его равным `1`. Исходный смысл kind
и owner byte не расширяется предположениями.

Copy унаследован и не переносит runtime-поля. Clone регистрирует новую пару
и сохраняет constructor defaults. Общий Reset меняет только поля базы,
сохраняя собственные `3C/3D/3E` и selector. Destructor использует общую базу.

[WinxVulnerableStateTests](../../../Winx/Tests/wxVulnerableStateTests.cpp)
проверяет lifecycle, RTTI, Clone/Reset, ключи, completion и порядок побочных
эффектов. PC сравнение исполняет собственные методы, base release/queue,
виртуальный update и consuming completion. Foreign animation/logger и
заимствованные графы имеют явные fixtures. PS2 scalar prefixes/suffixes
проверяют собственные masks, branches, stores и границы внешних вызовов;
полный EE и связанная игровая сцена остаются вне этой проверки.
