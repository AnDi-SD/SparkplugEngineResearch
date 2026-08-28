# Debug menu и маршруты runtime-проверки Winx Club

Статус: инвентарь PC debug menu подтверждён статически и живым запуском
28 августа 2026 года. Полный pristine baseline всех уровней ещё не выполнен.

## Проверенные сборки

| Роль | SHA-256 | Размер |
|---|---|---:|
| исходный PC `WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | 22 065 152 |
| PC `WinxClubDebug.exe` | `C27EA9DB4228781A12A90AE808807D4AF1397A7E40DD8F5FFF28F3C87CC62CDB` | 22 065 152 |

Копии debug executable в `local-data/Winx Club` и
`local-data/WinxClubWithDebugMenu` побайтно совпадают. Patch перехватывает F1
через `GetAsyncKeyState(0x70)` и вызывает штатные функции открытия/закрытия
debug menu. Само меню управляется scan codes стрелок и Enter. Обычные
synthetic virtual-key события достаточны для F1, но ненадёжны для навигации.

Окно игры также нельзя искать только через .NET `MainWindowHandle`: у живого
процесса он был равен нулю. Надёжный стенд перечисляет `EnumWindows`, сверяет PID
через `GetWindowThreadProcessId` и выбирает видимое top-level окно. Это свойство
стенда автоматизации, а не формата SMO.

## Фактическое корневое меню

Порядок подтверждён снимком живой PC-игры:

| № | Пункт | Форма |
|---:|---|---|
| 1 | `LOAD LEVEL` | выбор одного из 37 level descriptors и запуск |
| 2 | `ALFEA STATE(DAY)` | действие |
| 3 | `ALFEA STATE(BROKEN)` | действие |
| 4 | `PROFILER` | подменю |
| 5 | `HELPERS` | подменю |
| 6 | `DUMP TEXTURE LIST` | действие |
| 7 | `DUMP SOUND LIST` | действие |
| 8 | `DUMP SCENE GRAPH` | действие |
| 9 | `DUMP LEVEL BLOCKS` | действие |
| 10 | `DUMP ALL BLOCKS` | действие |
| 11 | `DUMP MEMORY STATS` | действие |
| 12 | `SHOW STATISTICS 0` | переключатель |
| 13 | `SHOW FPS 0` | переключатель |
| 14 | `TOGGLE WIRE FRAME` | действие |
| 15 | `VIBRATION TEST 0` | переключатель/тест |
| 16 | `DUMP CHARACTER USAGE` | действие |
| 17 | `SHOW DMA SIZE` | действие/индикатор |
| 18 | `RENDER OPTIMISATIONS 0` | переключатель |
| 19 | `CENTRE SCREEN` | действие |

Подменю `PROFILER` в проверенном запуске содержало `VISIBLE 0`, `RESET` и
`MAX TICKS 18`. Число 18 — наблюдавшееся runtime-значение, а не константа
формата. Подменю `HELPERS` содержало `INVINCIBILITY 0` и `MAGIC 0`.

Пункты dump пока только инвентаризированы: их ещё нельзя считать file trace.
Особенно полезны будущие пары `DUMP SCENE GRAPH` + список реально открытых
файлов и `DUMP CHARACTER USAGE` + активные ANM/SAN.

## Что именно делает `LOAD LEVEL`

PC-конструктор меню около `0x005CCF6A..0x005CCFBA` добавляет 37 записей из
штатной level table. Callback около `0x005CC310` получает zero-based выбранный
индекс, увеличивает его на единицу и передаёт level manager. Поэтому пункт не
принимает путь SMO и не перечисляет весь каталог ресурсов.

Callback отдельно обрабатывает диапазоны Алфеи 27–29, 30–32 и 33–35, выставляя
нужное состояние мира перед запуском уровня. В живом запуске Enter на начальном
пункте довёл игру до отрисовки Gardenia с HUD; `GameStateLog.txt` прошёл loading
state 74 и вернулся в основной state 1. Этот проход использовал рабочую папку
`local-data/Winx Club`, поэтому он подтверждает исправность debug-menu стенда,
но не заменяет pristine baseline.

Ключевое ограничение:

- debug menu непосредственно выбирает только ID 1–37;
- `startLevel` дополнительно позволяет обратиться к ID 41–49;
- 38–40 — пустые native descriptors;
- 50 содержит имя `Gardenia04`, но не образует полный загружаемый уровень;
- отдельного пункта для произвольного SMO, персонажа, SAN/ANM или test world нет.

## Native level matrix PC/PS2

В таблице `N` означает номер внутри указанного диапазона. Пути приведены без
корня PC `Media` и PS2 `data`; регистр PS2-путей нормализован базой корпуса.

| ID | Native name | PC level resources | PS2 PCK | Примечание |
|---:|---|---|---|---|
| 1–3 | `Gardenia01..03` | `Levels/Gardenia/Gardenia0N.{smo,spt,spl}` | есть | полные triplets |
| 4–8 | `Domino01..05` | `Levels/Domino/Domino0N.{smo,spt,spl}` | есть | полные triplets |
| 9–13 | `BMS01..05` | `Levels/Swamp/BMS0N.{spt,spl}` + `BMS_0N.smo` | есть | PC дополнительно содержит `BMS_04B.smo`; в PS2 его нет |
| 14–16 | `Cloud01_01..03` | `Levels/Cloud01/Cloud01_0N.{smo,spt,spl}` | есть | полные triplets |
| 17–21 | `Cloud02_01..05` | `Levels/Cloud02/Cloud02_0N.{smo,spt,spl}` | есть | полные triplets |
| 22 | `Cloud02_06` | полный triplet | отсутствует | строка descriptor есть и в PS2 executable, но файлов в PS2 PCK нет |
| 23–26 | `RedF_01..04` | `RedF_0N.{spt,spl}` + `RedF0N.smo` | есть | `RedFountain.spt` — дополнительный PC/PS2 script resource |
| 27–29 | `Alfea01_01..03` | `Alfea01_0N.{spt,spl}` + `Alfea0N.smo` | есть | day state |
| 30–32 | `AlfeaNight_01..03` | `AlfeaNight_0N.{spt,spl}` + `Alfea_night_0N.smo` | есть | night state |
| 33–35 | `AlfeaBroken_01..03` | `AlfeaBroken_0N.{spt,spl}` + `Alfea_broken_0N.smo` | есть | broken state |
| 36–37 | `Sky01..02` | `Sky0N.{spt,spl}` + `Sky_0N.smo` | есть | полные triplets |
| 38–40 | пусто | отсутствуют descriptors | недоступно | штатный запуск запрещён |
| 41–43 | `star_01..03` | `Levels/Challenges/star_0N.{smo,spt,spl}` | есть | только `startLevel`, не список debug menu |
| 44–46 | `race_01..03` | `Levels/Challenges/race_0N.{smo,spt,spl}` | есть | только `startLevel`, не список debug menu |
| 47–49 | `battle_01..03` | `Levels/Challenges/battle_0N.{smo,spt,spl}` | есть | только `startLevel`, не список debug menu |
| 50 | `Gardenia04` | есть `.spt/.spl`, нет `.smo` | triplet отсутствует | descriptor string есть в обоих executable, запуск неполон |

Таким образом, PC имеет 46 полных маршрутов `startLevel`: 1–37 и 41–49. В
PS2-корпусе из этих triplets отсутствует только `Cloud02_06`, то есть реально
наблюдаются 45. Наличие имени в executable само по себе не доказывает наличие
ресурса — `Cloud02_06` и `Gardenia04` дают два прямых контрпримера.

## Как теперь запускать проверки

Есть три разных маршрута, и их нельзя смешивать в одном выводе:

1. Debug menu подходит для ручной загрузки 37 PC-уровней и диагностических
   dump/toggle функций.
2. `SmoNativeValidator` с `Contextual` и временным `startLevel` нужен для
   воспроизводимого запуска ID 1–37 и 41–49 и перехвата конкретного штатного
   запроса ресурса.
3. `FastGeneric` подменяет ранний `Menus/mousecursor.smo` и быстро проверяет
   FFPS loader/object construction, но не доказывает работоспособность модели в
   её игровом контексте.

Персонажи и костюмы должны проверяться через уровень/сценарий, который их
запрашивает, либо через contextual resource interception. Меню проверяются
обычным startup. Для `test_world_*.smo` штатный отдельный маршрут пока не найден;
их нельзя включать в baseline как «доступные из debug menu».

## Матрица baseline

Авторитетные проходы используют `pc-pristine` как Media source и
изолированный временный `winx.ini`:

- ID 1 как контроль загрузки уровня;
- ID 22 как PC-only контроль и отрицательный PS2-контроль;
- ID 27, 30 и 33 как три состояния Алфеи;
- ID 41, 44 и 47 как три hidden challenge family;
- повтор ID 1 после каждого изменённого теста.

`RT-SMO-HEADER-*` выполнен отдельными fixed-size изменениями `0x04`, `0x08` и
`0x10` без изменения pristine-файлов. Результаты, hashes и native locators:
[`smo-runtime-results.md`](smo-runtime-results.md).

## Исправление изоляции и первый fast baseline

Первый запуск pristine executable обнаружил, что игра всё равно прочитала
глобальный registry `MediaPath` и построила путь в рабочую установку. Проверяемый
`mousecursor.smo` был безопасно подменён pristine-копией, но окружающий contextual
граф не был бы pristine. Валидатор исправлен: каждый нетаргетный результат
`BuildAssetPath` переводится в `Media` рядом с явно выбранным executable только в
памяти owned child process; не переводимый путь даёт `PathError`.

Повторный `FastGeneric` с
`pc-pristine/Media/Menus/mousecursor.smo` прошёл `CP02`, `CP03`, `FFPS01`,
`FFPS02`, `FFPS03`, вернул ненулевой native resource и выдержал двухсекундное
окно стабильности. Verbose trace одновременно подтвердил перевод startup-
ресурсов, шрифтов, SFX, Bloom SMO и SAN в `pc-pristine/Media`.
