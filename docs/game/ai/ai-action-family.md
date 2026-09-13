# `wxAIAction`: общий контракт и 35 производных действий

Семейство из36 классов независимо присутствует в PC и PS2. PC: все36 factory
и clone вернулись;35 классов прошли удаление clone и original. В36-м, Icy,
сохранён отдельный teardown failure. Всего178 вернувшихся class operations;
подготовка внешней сцены в это число не включена. PS2:36 factory allocation
literals,14 constructor windows и36 независимо разрешённых vtable identities.
Поведение конкретных атак/движения этим первым проходом не закрывается.

## Общая база и подготовка PC

| Поле/блок | PC offset | PS2 offset | Наблюдение |
| --- | ---: | ---: | --- |
| Текущее дочернее действие | `10` | `10` | null после ctor, notification forwarding |
| Контейнер дочерних объектов | `14` | `14` | разные compiler layouts `0C/10` |
| Два следующих pointer-sized поля | `20/24` | `24/28` | zero defaults, полные типы открыты |
| Встроенный PathFinder | `2C` | `30` | original ctor, null graph в пустой сцене |
| Следующий блок `300` байт | `54..353` | `58..357` | PC ctor оставляет allocator fill; назначение открыто |
| Семь words | `354..36C` | `358..370` | zero defaults |
| Integer default | `370` | `374` | `2000` decimal |
| Сбрасываемый word | `374` | `378` | default0, отдельный clear hook |
| Три float3 копии | `378/384/394` | `37C/388/398` | из globals`7600E0/476F50` |
| Word между векторами | `390` | `394` | zero default |

PC хвост`3A0/3A4` также не заполнен constructor. Неинициализированные области
не объявляются нулевыми. PS2 static stores независимо показывают смещение
базы; из одинакового classID не выводится автоматическое совпадение всех полей.
Во всех36 factory PS2 allocation на4 байта больше PC, включая большой
`wxKikoMovingAIAction` (`1BF8/1BFC`).

Original notification `58EA60/223400` при code`1C` сначала вызывает собственный
source-level v9 (PC byte slot`24`, PS2`2C`), затем заново читает current`+10`
и, если он ненулевой, передаёт **тот же message pointer** его notification
virtual. При другом code собственного v9-вызова нет, forwarding сохраняется.
10 paired cases: codes0,27,28,29,`FFFFFFFF`, каждый с null/non-null child.
PC выполняет весь метод; PS2 ordinary prefix начинается после SQ prologue
с явно заданными входами и заканчивается до epilogue. Все virtual target
bodies — original true/no-op leaves, hooks лишь наблюдают вызовы.

## Производные конструкции

Attack, Frog, FishWander, Wander и KikoMoving имеют дополнительные inline
PathFinder constructions. Простые Idle и DroidWander имеют размер самой базы
и заменяют vtable. Hurt добавляет один false byte. Другие короткие attack
constructors заполняют собственные flags, counters, pointers и literal defaults;
значения не превращены в выдуманные игровые enum или имена методов.

## Icy: граница корректного lifecycle ещё не установлена

PC `wxIcyAttackAIAction` ctor`5C1370` и clone`5C1C60` возвращаются.
Clone destructor`5C1CB0` сначала удаляет5 созданных owners, затем читает
два указателя `+3EC/+3F0`, не заполненных этим constructor. При explicit
allocator fill`CC` первый из них приводит к read`CCCCCCCC` в`5C1D96`.
Initial/clone snapshots и completed stages сохранены до отказа.

## Размеры

Шестнадцатеричные размеры original PC allocation и PS2 factory literal.

| Класс | PC | PS2 | PC teardown |
| --- | ---: | ---: | --- |
| wxAIAction | `3A8` | `3AC` | passed |
| wxAttackAIAction | `3F4` | `3F8` | passed |
| wxBacoAttackAIAction | `3CC` | `3D0` | passed |
| wxDarcyAttackAIAction | `460` | `464` | passed |
| wxDroidAttackAIAction | `3E8` | `3EC` | passed |
| wxDroidWanderAIAction | `3A8` | `3AC` | passed |
| wxFishWanderAIAction | `418` | `41C` | passed |
| wxFlyingWanderAIAction | `3E0` | `3E4` | passed |
| wxFrogAttackAIAction | `3F0` | `3F4` | passed |
| wxGhoulScriptAIAction | `3BC` | `3C0` | passed |
| wxGolemAttackAIAction | `3C0` | `3C4` | passed |
| wxHelpAIAction | `468` | `46C` | passed |
| wxHurtAIAction | `3AC` | `3B0` | passed |
| wxIceGargoyleAttackAIAction | `3E8` | `3EC` | passed |
| wxIceGargoyleClawAIAction | `3B8` | `3BC` | passed |
| wxIceGargoyleSleepingAIAction | `3B0` | `3B4` | passed |
| wxIceGargoyleWithdrawlAIAction | `3B8` | `3BC` | passed |
| wxIceWormAttackAIAction | `3D0` | `3D4` | passed |
| wxIceWormHolesAIAction | `3C8` | `3CC` | passed |
| wxIcyAttackAIAction | `480` | `484` | blocked |
| wxIdleAIAction | `3A8` | `3AC` | passed |
| wxKikoHoleAIAction | `3BC` | `3C0` | passed |
| wxKikoMovingAIAction | `1BF8` | `1BFC` | passed |
| wxKnutAttackAIAction | `408` | `40C` | passed |
| wxMikaelWandringAIAction | `3E8` | `3EC` | passed |
| wxMinotaurAttackAIAction | `3C8` | `3CC` | passed |
| wxMinotaurDefendAIAction | `3B8` | `3BC` | passed |
| wxMosquitoAttackAIAction | `3CC` | `3D0` | passed |
| wxSpiderAttackAIAction | `3DC` | `3E0` | passed |
| wxStormyAttackAIAction | `470` | `474` | passed |
| wxTrollAttackAIAction | `450` | `454` | passed |
| wxTrollBeforeFightAIAction | `3B8` | `3BC` | passed |
| wxWanderAIAction | `410` | `414` | passed |
| wxWinxFlyAIAction | `3C0` | `3C4` | passed |
| wxYetiAttackAIAction | `494` | `498` | passed |
