# Debug menu и маршруты runtime-проверки Winx Club

## Фактическое корневое меню

Порядок подтверждён снимком живой PC-игры:

| № | Пункт | Форма |
| ---: | --- | --- |
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

Пункты dump пока только инвентаризированы: их ещё нельзя считать file trace.
Особенно полезны будущие пары `DUMP SCENE GRAPH` + список реально открытых
файлов и `DUMP CHARACTER USAGE` + активные ANM/SAN.

## Что именно делает `LOAD LEVEL`

PC-конструктор меню около `0x005CCF6A..0x005CCFBA` добавляет 37 записей из
штатной level table. Callback около `0x005CC310` получает zero-based выбранный
индекс, увеличивает его на единицу и передаёт level manager. Поэтому пункт не
принимает путь SMO и не перечисляет весь каталог ресурсов.

Ключевое ограничение:

- debug menu непосредственно выбирает только ID 1–37;
- `startLevel` дополнительно позволяет обратиться к ID 41–49;
- 38–40 — пустые native descriptors;
- 50 содержит имя `Gardenia04`, но не образует полный загружаемый уровень;
- отдельного пункта для произвольного SMO, персонажа, SAN/ANM или test world нет.

## Native level matrix PC/PS2

| ID | Native name | PC level resources | PS2 PCK | Примечание |
| ---: | --- | --- | --- | --- |
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

Есть три разных маршрута, и их нельзя смешивать в одном выводе:

Персонажи и костюмы должны проверяться через уровень/сценарий, который их
запрашивает, либо через contextual resource interception. Меню проверяются
обычным startup. Для `test_world_*.smo` штатный отдельный маршрут пока не найден;
их нельзя включать в baseline как «доступные из debug menu».

## Матрица baseline

Результаты, hashes и native locators: `smo-runtime-results.md`.

## Исправление изоляции и первый fast baseline

Валидатор исправлен: каждый нетаргетный результат `BuildAssetPath` переводится в `Media` рядом с явно выбранным executable только в памяти owned child process; не переводимый путь даёт `PathError`.
