# Семейство `wxGameFlowState`: PC lifecycle и PS2 construction

Первый проход по 34 потомкам общей базы: 33 concrete factory и один abstract
`wxDialogueBoxGameFlowState`. PC: 32 original factory → RTTI getter → clone →
delete clone → delete original завершились, всего 160 операций. OptionMenu
остановлен на отсутствующей внешней структуре до возврата factory. PS2:
независимо исследованы 33 factory, 18 общих constructor bodies и 34 vtable
identity. Это начальное исследование устройства классов; активное поведение
меню, диалогов и уровней этим проходом не закрывается.

## Общие основания

Каждый PC класс проверен в fresh bounded guest: micro100000 instructions/2 с
на операцию, 30 с на child; allocator/SEH/clock/clone-map — существующая обвязка.
Задано borrowed `wxUserInput` storage: owner `+18` ссылается на 16 нулевых
байт ввода. Это явная предпосылка, а не исполненный input/HUD/game startup.
Original factory, derived constructor/destructor и timer выполняются сами.
Состояние сохраняется до clone и teardown, чтобы последующая ошибка не
уничтожала результат ранее вернувшихся операций.

После четырёх пилотных классов оставшиеся 29 обработаны тремя процессами
за 12,527 с wall time. Каждому child заданы собственные пределы; объём
guest heap ограничен 64 КиБ, отдельного allocation — 32 КиБ. Повторять всю
семью ради новой детали не требуется: дополнительные проверки адресные.

Все 32 PC clone используют inherited empty copy `40ECE0`; они создают новое
состояние с constructor defaults и собственным base timer, а не копируют
runtime payload. На PS2 все 34 vtable независимо указывают empty copy
`100320` в source-level slot `+14`. PS2 vtable найден по literal store
constructor/factory, затем проверен отдельным RTTI getter, возвращающим
именно registration record класса. Таблицы не сопоставлялись только по именам.
Raw table/getter bytes, allocation literals, PC initial/clone bytes и SHA
источников сохранены в `research/game-flow-family-contracts-2026-09-10.json`.

PS2 окна — линейные дизассемблированные original bytes; они не выдаются за
исполненный R5900 lifecycle или полный CFG. Inlined construction пяти factory
дочитан расширенными окнами. Обе платформы независимо подтверждают различные
размеры ряда диалоговых/меню классов: переносить PC offsets целиком нельзя.

## Проверенные общие конструкции

`wxMenuGameFlowState` добавляет к базе: `+3C=0`, `+40=1`, `+44=1`,
`+48/+4C/+50/+54=0`, byte `+58=0`; затем подписывается на key32,
заимствует input member `+18` в `+54` и создаёт второй timer в `+50` с Reset/Start.
PC original ctor `5F1260`, PS2 `34A970`. По этим вызовам обнаружено и отдельно
[исправлено расхождение менеджера подписок](native-subscription-order-correction-2026-09-10.md).
В отдельном PC Menu probe после удаления состояния и clone два оставшихся
32-байтных allocation — global manager `75537C` и его пустой tree sentinel;
group count равен нулю. Это не полный teardown игрового singleton.

PS2 `wxDialogueBoxGameFlowState` ctor `327C50` доступен через дочерние классы,
хотя registered factory отсутствует. Подтверждены bytes `3C/3D=0`,
`3E/3F=1`; words `40/44/48/4C=0`, `50/54=2`, `58=13`, `5C/60=0`, `64=35`,
container helper для `68`, `78/7C=0`, byte `80=0`; subscription key14,
borrowed input `7C` и buffers в `40/44` размером 64/2048 байт. Типы buffer
elements и смысл всех selector constants пока не объявляются установленными.

PS2 `wxPlayingLevelGameFlowState` ctor `35F980` сбрасывает общий 56-pointer
cache и записывает флаги по `16C/16E/170/172`. `wxChallengeGameFlowState`
ctor `31DC80` добавляет собственные arrays, `1C0=41`, byte `1FC=1`,
`200=5000`; Battle ctor `31A710` меняет `1C0=43`, `1B0=173`, `1B4=172`,
`1B8=171`, `200=0`, обнуляет собственные pointers/flags. Эти literal IDs
сохранены без неподтверждённых имён игровых enum.

Конструкторы menu descendants задают пары `+40/+44`: Continue `12/1`,
Diary `14/2`, Fashion `16/2`, HertzChoice `70/2`, Inventory `33/2`,
Library `22/1`, Magic `15/2`, Option `17/2`, PromptSave `30/8`.
Это offsets и literal values, не доказательство полного сценария выбора.

`wxDebugMenuGameFlowState`, `wxDialogWindowGameFlowState`,
`wxLoadSaveGameFlowState` имеют размер базового состояния `3C`;
первый проход не обнаружил собственных constructor fields.
`wxCentreScreenGameFlowState` выделяет `48`, однако PC constructor
оставляет `3C/40/44` нетронутыми (`CC` в контролируемом allocator).
Заменять их предположительно нулевыми defaults нельзя.

## Ограничение OptionMenu и границы оценки

PC factory `5E5A60` не вернулась: invalid read4 в `56064B` после входа в
внешнюю структуру. Capture сохраняет last tail, allocations и отсутствие
factory-return. Это не исчерпание instruction/heap cap; повышение лимита
не исправляет отсутствующую зависимость. Guest не возобновлялся и callee
не заменялся выдуманным успешным constructor. Для PC OptionMenu behavioral
оценка за этот проход не начисляется. PS2 direct ctor/factory изучены отдельно.

Первые оценки: по20/100 для 32 успешных PC lifecycle и по15/100 для34
PS2 static constructions. Они отражают изученную начальную часть класса,
не процент его готовой реализации или число закрытых методов. PC abstract
DialogueBox не получает автоматический кредит от успеха потомков.
Открыты active hooks, notification consumers, переходы/ресурсы, ownership
в активном графе, callback mutation и платформенные зависимости.

Evidence: `local-data/results/native-cycle-20260910-1900/game-flow-family/`;
полный перечень и SHA в contracts JSON. В нём сохранены первоначальная версия
PC probe и адресный Menu run2; подсчёт160 операций относится только к run1.

## Размеры и непосредственные базы

Размеры ниже шестнадцатеричные. PC — фактический allocation успешно созданного
объекта; PS2 — literal первого factory allocation. `—` означает отсутствие
успешного измерения, а не нулевой размер.

| Класс (без `wx` и `GameFlowState`) | База | PC | PS2 |
|---|---|---:|---:|
| BGMenus | Menu | `60` | `60` |
| BattleChallenge | Challenge | `25C` | `25C` |
| CentreScreen | GameFlowState | `48` | `48` |
| Challenge | PlayingLevel | `208` | `208` |
| Choice | DialogueBox | `C4` | `C0` |
| Cinematic | GameFlowState | `E4` | `E4` |
| ContinueMenu | Menu | `28C` | `288` |
| CreditsMenu | GameFlowState | `44` | `44` |
| DateSelection | DialogueBox | `A0` | `9C` |
| DebugMenu | GameFlowState | `3C` | `3C` |
| DialogWindow | GameFlowState | `3C` | `3C` |
| DialogueBox | GameFlowState | — | — |
| DiaryMenu | Menu | `1EC` | `1E0` |
| FashionMenu | Menu | `2D8` | `2D8` |
| FirstDate | GameFlowState | `8C` | `8C` |
| GameOver | Menu | `60` | `60` |
| HertzChoiceMenu | Menu | `BC` | `B8` |
| InventoryMenu | Menu | `3DC` | `3D8` |
| Library | Menu | `148` | `144` |
| LoadSave | GameFlowState | `3C` | `3C` |
| LogoAnim | GameFlowState | `88` | `88` |
| MagicMenu | Menu | `1A4` | `1A0` |
| MainMenu | Menu | `6C` | `6C` |
| Menu | GameFlowState | `5C` | `5C` |
| NewGameMenu | ContinueMenu | `29C` | `298` |
| OneLiner | DialogueBox | `D0` | `D0` |
| OptionMenu | Menu | — | `2D4` |
| PlayingLevel | GameFlowState | `188` | `188` |
| PressStart | Menu | `8C` | `8C` |
| PromptDelete | Menu | `80` | `7C` |
| PromptSave | Menu | `8C` | `88` |
| RacingChallenge | Challenge | `220` | `220` |
| SignTutorial | DialogueBox | `A4` | `9C` |
| StarsChallenge | Challenge | `228` | `228` |
