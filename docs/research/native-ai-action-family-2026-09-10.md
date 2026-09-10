# `wxAIAction`: общий контракт и 35 производных действий

Семейство из36 классов независимо присутствует в PC и PS2. PC: все36 factory
и clone вернулись;35 классов прошли удаление clone и original. В36-м, Icy,
сохранён отдельный teardown failure. Всего178 вернувшихся class operations;
подготовка внешней сцены в это число не включена. PS2:36 factory allocation
literals,14 constructor windows и36 независимо разрешённых vtable identities.
Поведение конкретных атак/движения этим первым проходом не закрывается.

## Общая база и подготовка PC

`wxAIAction` имеет ID`490A6EB5`, base`spBaseObject`, размер PC`3A8`/PS2`3AC`,
factory`590050/223850`, ctor`58FF00/2236A0`, vtable`702450/492060`.
Первый fresh PC factory достиг micro cap100000. Следующий fresh file profile
достиг1M instructions. Fresh protected-constructor profile6M/24s прошёл
защищённый bootstrap и остановился через2,374,310 инструкций на отсутствующем
корне сцены: read`14` в`4F2741`. Эти три отказа сохранены; guest не возобновлялся.

Истинная зависимость — встроенный `wxPathFinder`, PC`+2C`/PS2`+30`, размер`28`.
Он ищет `Navigation Graph` и затем `nv_group` через корень текущей сцены.
PC: core global`755274`, core`+18` → scene`+14` → root, search virtual`+2C`.
PS2: core global`49F850`, те же pointer fields, search virtual`+30`.
Return первого поиска должен удовлетворять classID`188A161F` (`spNavigationGraph`
в независимых PC/PS2 registration records), иначе путь
сохраняет null. Полный граф/успешный поиск и обработка его данных не проверены.

Для fresh successful probes явно заданы borrowed core/scene storage и
**настоящий пустой `spNode`**, созданный original factory`421E20` после original
matrix static initializer`6D38E0`. Его original search возвращает отсутствие
навигационного графа. Ни ctor `wxPathFinder`, ни Node search не заменены.
Это проверка сценария пустой сцены; original engine/scene startup и полноценный
уровень не заявляются. Использована прежняя allocator/SEH/clock/clone-map
обвязка. После original/clone удаляется и подготовленный root.

Base AIAction с этой предпосылкой создаётся за3990 instructions вместо
повторного bootstrap EngineCore. Pilot Attack прошёл отдельно, остальные35
классов обработаны3 bounded процессами за17,619 с;34 из batch прошли полностью,
один сохранил teardown failure. Каждый class guest — micro100000/2s на
операцию,30s child,64KiB heap/32KiB allocation. Полная семья не перезапускается
из-за отдельной детали.

## Проверенные поля и общие методы

| Поле/блок | PC offset | PS2 offset | Наблюдение |
|---|---:|---:|---|
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

Дополнительно paired проверены clear`58EA50/223490` (меняет ровно4 байта),
true hooks, void hooks и false hook. Void leaf PC`48EAA0`/PS2`223240` не
объявляется возвращающим false: значение return register здесь игнорируется.
Base copy`58FDD0/223250` имеет собственный cleanup destination path;
успех clone на constructor-default source не доказывает правильный перенос
изменённого payload. У многих потомков собственные copy slots; они сохранены
в contracts, автоматическое заявление об одинаковом copy всех классов отсутствует.

## Производные конструкции

Attack, Frog, FishWander, Wander и KikoMoving имеют дополнительные inline
PathFinder constructions. Простые Idle и DroidWander имеют размер самой базы
и заменяют vtable. Hurt добавляет один false byte. Другие короткие attack
constructors заполняют собственные flags, counters, pointers и literal defaults;
значения не превращены в выдуманные игровые enum или имена методов.

GhoulScript, IceWormHoles, KikoHole/KikoMoving, Darcy, Icy, Stormy и Troll
имеют subscription-related construction. Icy создаёт5 owners по`1C` байт;
Yeti и крупные witch actions имеют отдельные intrusive/resource fields.
Полные resource lifetime и callbacks активных игровых графов открыты.
Проверка освобождения class allocation не утверждает, что весь процессный
singleton graph закрыт: оставшиеся выделения перечислены отдельно в captures.

## Icy: граница корректного lifecycle ещё не установлена

PC `wxIcyAttackAIAction` ctor`5C1370` и clone`5C1C60` возвращаются.
Clone destructor`5C1CB0` сначала удаляет5 созданных owners, затем читает
два указателя `+3EC/+3F0`, не заполненных этим constructor. При explicit
allocator fill`CC` первый из них приводит к read`CCCCCCCC` в`5C1D96`.
Initial/clone snapshots и completed stages сохранены до отказа.

PS2 ctor`25AAB0` также не заполняет соответствующие `+3F0/+3F4`, а destructor
`25A540` читает их в отдельном цикле2 элементов после цикла5 owners.
Это независимое static подтверждение обращения, не воспроизведённый PS2 crash.
Такой результат не доказывает ошибку самой игры: остаются normal init path
между factory и destruction и native allocator contract. Требуется найти
producer этих полей/обычную игровую последовательность. Произвольные defaults
в общий код не добавлены; полный lifecycle Icy пока не засчитан.

## Исправленные ошибки исследования и evidence

Ранняя привязка embedded ctor к `wxPerception` по соседнему RTTI getter была
неверна. Прямые PC vtable`6F6300` → getter`4F21E0` → record`74E548` и PS2
vtable`49B610` → getter`3F98E0` → record`4C5330` независимо дают **wxPathFinder**.
Сохранён `embedded-pathfinder-identity.json`; исторические raw window labels
`perception-*` не являются утверждением identity. Проверенный PathFinder factory
`4014C0/3F7AA0` выделяет`28`: PS2 literal и отдельный PC original factory
независимо подтверждают размер. PC PathFinder factory/RTTI/clone/delete clone/
delete original и подготовка/удаление Node завершились за0,948 с; алгоритмы
поиска по непустому графу остаются открыты.

В base-leaves run1 успешно записаны11 paired cases (10 notification+clear),
после чего стенд ошибочно пометил прямой PC true leaf как событие self-v9
и провалил собственное сравнение меток. Это ошибка наблюдателя, не original
execution fault. Source run1 сохранён; после исправления отдельно выполнены
оставшиеся5 leaves в run2 за0,776 с. Предыдущие11 не перепроверялись без причины.

Каталог evidence: `local-data/results/native-cycle-20260910-1900/ai-action/`.
Основные файлы: `pc-*-empty-scene-run1.json`, `batch-processes.json`,
`ps2-factories/capture.json`, `ps2-factories-extended/capture.json`,
`ps2-constructors/capture.json`, `ps2-vtable-candidates.json`, `base-leaves-run1/2.json`,
`base-leaf-windows/capture.json`, `icy-windows/capture.json`,
`icy-ctor-window/capture.json`, сохранённые версии обоих probes.
Raw table/getter bytes и SHA каждого class capture собраны в
`research/ai-action-construction-contracts-2026-09-10.json`.

Первые assessments: base AIAction35/30;34 остальных successful classes20/15;
Icy15/15 с отдельным неизвестным lifecycle. Это начальное исследование,
не35 закрытых классов и не завершённые AI algorithms. Полные атаки, поиск пути,
выбор цели, движение/анимация, активные resource graphs и normal init sequences
остаются последующими контрактами.
Связанному PathFinder отдельно даны20/15 за его собственное evidence,
не за одно упоминание в AIAction. Он не включён в число36 членов семьи.

## Размеры

Шестнадцатеричные размеры original PC allocation и PS2 factory literal.

| Класс | PC | PS2 | PC teardown |
|---|---:|---:|---|
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
| wxShadowBeastAttackAIAction | `3E0` | `3E4` | passed |
| wxSpiderAttackAIAction | `3DC` | `3E0` | passed |
| wxStormyAttackAIAction | `470` | `474` | passed |
| wxTrollAttackAIAction | `450` | `454` | passed |
| wxTrollBeforeFightAIAction | `3B8` | `3BC` | passed |
| wxWanderAIAction | `410` | `414` | passed |
| wxWinxFlyAIAction | `3C0` | `3C4` | passed |
| wxYetiAttackAIAction | `494` | `498` | passed |
