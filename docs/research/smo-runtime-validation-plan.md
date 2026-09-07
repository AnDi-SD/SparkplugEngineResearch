# План быстрой runtime-валидации SMO

Статус: сохранённый план игровых проверок; исходная точка — 29 августа2026.
На 6 сентября2026 активен отдельный PC-first native reconstruction cycle;
см. [текущий порядок](native-reconstruction-plan.md) и
[журнал](../../journal/2026/2026-09-06-pc-reconstruction-until-1000.md).
Guest-only instruction tests не считаются выполнением in-game пунктов ниже.

> Исторический pre-release scope от 29 августа2026 ограничен gate для
> SmoLVLcreator и production model import. Сокращённая граница, порядок и
> критерии выпуска находятся в
> [`smo-lvlcreator-import-mvp-plan.md`](smo-lvlcreator-import-mvp-plan.md).
> Остальные разделы сохранены как backlog; этот scope не ограничивает более
> поздние native research cycles и сам по себе не разрешает запуск игры/релиз.

## Точка возобновления — 29 августа 2026 года

Уже закрыты грубые loader-риски заголовка и базовая семантика имён:

- `RT-SMO-HEADER`: версия сериализатора, platform mask и безопасные границы поля
  `0x08` проверены нативным загрузчиком;
- `RT-SMO-NAME-CASE`: PC runtime выполняет регистрозависимый exact lookup;
- `RT-SMO-NAME-MISSING`: отсутствующие parent и leaf target не отвергают весь SMO;
  новый key получает отдельный binding slot, а descendant registry сохраняется;
- `RT-SMO-NAME-DUPLICATE`: дубликаты имён загружаются и на binding-слое работают
  как all-target: два разных evaluator получают один exact name/transform slot.

Полные hashes, locators, запуски и отрицательные результаты находятся в
[`smo-runtime-results.md`](smo-runtime-results.md), а машинные карточки — в
[`evidence/`](evidence/). Локальная основная база `smo-corpus-v2.sqlite` после
импорта этих карточек имеет SHA-256
`174F3F798180D76A526B591D9344D119AB1294D65C21DB76A02AB5EA4AB8C8B2` и проходит
`PRAGMA integrity_check`.

Продолжать в следующем порядке:

1. Через debug menu или обычный game flow довести Bloom до активного animation
   tick и снять PRS/world transforms pristine и missing parent. Binding и
   duplicate all-target уже закрыты; осталось только итоговое bind-pose/world
   inheritance, которое маршрут `startLevel=2` не активирует.
2. Выполнить `RT-SMO-ANM-SAN`: подмена только на существующий SAN, проверка
   пересечения track names, длительности и `AdvBloom.anm` через реальный game
   flow/debug path.
3. Проверить `RT-SMO-TRANSFORM`, затем на фиксированной камере
   `RT-SMO-MODEL-ORDER` и `RT-SMO-MATERIAL`.
4. Перейти к миру: collision groups `1/2`, `wxFaceData`, BV, navigation и
   occlusion.
5. Проверить controllers, fog, light, particles, lens flare и sky box.
6. Завершить быстрый этап GUI anchors, runtime text и hit testing.
7. Повторить общие fixed-size контракты на PS2, когда тот же тест имеет
   наблюдаемый emulator/debug path без отдельной разработки инструментария.
8. Только после устойчивых same-size результатов начинать вторую волну простых
   optional fields.

Перед возобновлением выполнить smoke-набор: `SmoNativeValidator.Tests`,
`SmoViewer.FormatTests`, сборки Viewer/Inspect/CLI, `PRAGMA integrity_check` и
контрольный pristine native load. После каждого одиночного эксперимента обновлять
runtime results, open questions, class-документ, evidence JSON и corpus DB.

Долгие задачи — универсальный writer/object-directory rebuild, полный PS2
DMA/VIF/SAN, полный SPT/SPL и восстановление сложных serializer-ветвей — намеренно
не входят в эту очередь и остаются backlog разработки инструментов.

## Цель

Закрыть максимум вопросов, которые можно проверить непосредственно в игре через
debug menu, загрузку уровней, персонажей, меню и эффектов, не начиная
многонедельный реверс PS2 DMA/VIF или разработку универсального SMO writer.

Главный результат этапа — не количество запущенных файлов, а набор проверенных
контрактов, который не позволит снова принять нормальный platform-specific layout
за повреждение, строковый lookup за указатель или parse success за гарантию
совместимости с runtime.

Канонический перечень вопросов находится в
[`../../research/open-questions.md`](../../research/open-questions.md).
Выполненные карточки и точные evidence locators записываются отдельно в
[`smo-runtime-results.md`](smo-runtime-results.md).

## Границы этапа

В текущий этап входят:

- baseline-загрузка всего, что доступно из debug menu;
- PC runtime-тесты length-preserving/fixed-size изменений;
- короткие повторяемые проверки common PC/PS2 полей, если PS2-версию можно
  запускать с тем же уровнем наблюдаемости;
- визуальные, анимационные, collision, navigation и GUI-проверки;
- минимальные optional-field эксперименты только после доказательства базового
  structural mutation path;
- обновление class-документов и evidence базы после каждого закрытого вопроса.

В этот этап не входят:

- полный PS2 DMA/VIF и texture-swizzle decoder;
- поиск/создание 32-bit index-buffer ресурса;
- восстановление exporter palette splitter;
- универсальная пересборка object directory и произвольных relationship graphs;
- полный реверс SPT/SPL, PS2 SAN и всех D3D9 enum names;
- генерация отсутствующих сложных ветвей вроде BSP Polygon или нового
  multi-element LensFlare;
- массовый перенос объектов между несвязанными SMO.

Эти задачи выполняются позже вместе с importer/writer и не должны задерживать
быстрые runtime-проверки.

## Правила безопасного эксперимента

1. `pc-pristine` и `ps2-pristine` никогда не изменяются. Игра получает отдельную
   тестовую копию одного ресурса.
2. Один файл, один объект, одно поле и одна гипотеза на запуск. Комбинированные
   изменения запрещены до подтверждения одиночных.
3. Сначала выполняются только same-size изменения payload. Изменение длины,
   object count или relationship переносится в отдельную волну.
4. Перед запуском изменённый SMO обязан пройти строгий parser, field decoder,
   object-reference audit и byte-diff от baseline.
5. Имя файла/SAN/target выбирается только из реально существующего каталога;
   случайные имена не используются как доказательство loader-семантики.
6. После каждого изменённого запуска выполняется повтор исходного baseline. Это
   отделяет эффект поля от накопленного состояния, cache и случайного сбоя.
7. Crash считается результатом только вместе с точной стадией: открытие файла,
   создание объекта, вход в уровень, первый render, animation start или gameplay
   interaction.
8. Для каждого результата сохраняются hashes, locator поля, diff, способ запуска,
   build executable, screenshot/video/log и итог `confirmed/refuted/inconclusive`.

## Этап 0 — подготовить стенд

### 0.1. Инвентарь debug menu

Выполнено 28 августа 2026 года: фактическое меню, подменю, callback `LOAD LEVEL`
и PC/PS2 level matrix записаны в
[`winx-debug-runtime-inventory.md`](winx-debug-runtime-inventory.md).

Выяснено, что debug menu непосредственно выбирает только 37 PC-уровней. В нём
нет произвольного выбора SMO, персонажа, костюма, GUI scene или test world.
Challenge ID 41–49 доступны через скрытый `startLevel`; персонажей и остальные
ресурсы нужно покрывать обычным game flow либо contextual interception. File
trace для каждого маршрута остаётся частью baseline sweep, а не инвентаризации
самого меню.

### 0.2. Набор контрольных ресурсов

Минимальный обязательный набор:

- `bloom_jeans.smo` и связанные Bloom ANM/SAN — имена, skin, волосы и крылья;
- Kikko — PC split palettes против одной 64-slot PS2 palette;
- `Minautor.smo` и `knutBoss.smo` — непрозрачные/прозрачные/additive материалы;
- `Alfea02.smo` — BSP/static world placement;
- `Alfea03.smo` — тяжёлый partition/render/collision сценарий;
- `Gardenia01.smo` — прямая PC/PS2 mesh/bounds пара;
- `Gardenia03.smo` — реальное различие PC rootless против PS2 octree root;
- `menu.smo`, `igmenu_opt_pc.smo`, `gameover.smo` — font/text и runtime GUI
  anchors;
- по одному минимальному ресурсу с SphereBV, BoxBV, OBB, Fog, Light,
  ParticleSystem, AnimTexController, LensFlare, SkyBox, navigation и occlusion.

Последняя группа выбирается запросом к corpus DB: самый маленький SMO с нужным
классом и без несвязанных сложных ветвей. Это уменьшает неоднозначность crash.

### 0.3. Карточка эксперимента

Каждый тест получает стабильный ID вида `RT-SMO-AREA-NNN` и запись:

```text
id:
platform/build:
debug-menu entry:
baseline files + sha256:
object index/name/class:
section/field/occurrence:
old value -> new value:
expected discriminating outcomes:
strict parser/audit result:
runtime result:
baseline-repeat result:
evidence files:
conclusion:
documentation updated:
```

## Этап 1 — полный baseline sweep без изменений

Через debug menu последовательно открыть уровни 1–37. Challenge ID 41–49 открыть
через изолированный `startLevel`; меню — обычным startup; персонажей — через
уровень/сценарий, который реально запрашивает нужный вариант. Test worlds без
найденного штатного маршрута не считать частью debug-menu baseline. Для каждого
доступного маршрута проверить:

- файл загрузился и не упал;
- основные renderables видимы;
- texture/alpha/UV animation выглядит устойчиво;
- idle и минимум одна активная animation проигрываются;
- персонаж сталкивается с поверхностью и не проваливается;
- переходы/порталы/камеры работают;
- GUI-текст и hit targets совпадают;
- известные эффекты стартуют и завершаются.

Это создаёт карту реального покрытия ресурсов. SMO, который присутствует в
корпусе, но никогда не загружается найденными путями, не используется как
единственное runtime-доказательство.

## Этап 2 — FFPS loader и object identity

Приоритет: максимальный. Эти ошибки способны обесценить все последующие writer-
эксперименты.

### RT-SMO-HEADER

Статус: завершено 28 августа 2026 года; полные карточки и hashes находятся в
[`smo-runtime-results.md`](smo-runtime-results.md).

Проверено:

1. `0x04`: `0x26 -> 0x27` даёт native `Wrong file version` до object
   construction; это serializer version;
2. `0x10`: PC принимает `1/2/3` и отвергает PS2-only `8`; disassembly PC/PS2
   подтверждает маски common `1`, PC `2`, PS2 `8`;
3. `0x08`: ноль и переключение бита проходят на `mousecursor.smo`, ноль также
   проходит Bloom contextual с ANM/SAN; функция header validation поле не читает.

Точный generator/name поля `0x08` остаётся executable/exporter-вопросом, но для
loader/writer safety грубая неопределённость закрыта: оно не checksum, не
platform selector и не требуется для проверенных object graphs.

### RT-SMO-ID

Без изменения payload по очереди проверить на маленьком graph:

- изменение только directory object ID;
- согласованное изменение ID и одной reference;
- перестановку двух независимых directory entries без перемещения bodies.

Этот блок выполняется только если имеющийся инструмент умеет доказуемо сохранить
все ссылки и размеры. Иначе он сразу переносится в writer backlog.

## Этап 3 — имена ANM/SAN/SMO

Использовать отдельную копию Bloom и только same-length строки.

### RT-SMO-NAME-CASE

Статус: завершено 28 августа 2026 года. Same-length mutation
`R_Ankle -> r_Ankle` изменила один байт только в SMO и прошла native load, но
точный `char_traits<char>::compare`-поиск не нашёл равенства для нового ключа.
Pristine trace дал два точных `R_Ankle == R_Ankle`, mutation — ноль точных и
ноль cross-case совпадений для `r_Ankle`. PC lookup регистрозависим; отсутствие
падения не означает успешную привязку. Полная карточка находится в
[`smo-runtime-results.md`](smo-runtime-results.md#rt-smo-name-case-001--регистр-имени-node).

### RT-SMO-NAME-MISSING

Статус loader и binding: завершено 29 августа 2026 года. One-byte mutations
parent `[14] R_Ankle -> Z_Ankle` и leaf `[13] R_Toe -> Z_Toe` обе прошли
contextual load, `CP08`, ненулевой return и окно стабильности. Каждая переводит
ровно 168 tracks из exact в missing; parent trace не имеет ни exact, ни
cross-match. Transform-binding probe записал для новых имён отдельный слот
`0xD8` вместо прежних `R_Ankle/R_Toe`, а descendant `foot_right` сохранил exact
lookup и слот `0x46`. Остался только итоговый визуальный fallback: bind pose и
world-transform inheritance в активном animation tick.

### RT-SMO-NAME-DUPLICATE

Статус loader/key namespace/binding: завершено 29 августа 2026 года. Зеркальные
`R_Toe -> L_Toe` и `L_Toe -> R_Toe` mutations прошли load. Trace в обоих
порядках сворачивает дубликаты в один exact key и делает противоположный SAN
track missing. Binding-write probe показал два разных `spTransformTrackEval` с
одним слотом (`L_Toe=0x3B` или зеркально `R_Toe=0x3F`), поэтому на этом слое
политика all-target подтверждена, а first-only/last-only исключены. Final PRS
probe сохранён, но `startLevel=2` не вызывает evaluator до окончания окна.

### RT-SMO-ANM-SAN

- заменить SAN reference в ANM только на другой существующий SAN с тем же
  строковым размером либо через корректный length-aware editor;
- проверить пересечение track names, длительность и fallback;
- отдельно проверить `AdvBloom.anm` через debug/state path.

После оставшихся missing/duplicate/ANM-SAN тестов обновить таблицу
exact/case-folded/missing/ambiguous bindings и отделить service tracks от реально
потерянных bones. Уже доказанный case-folded-only результат считать ошибкой
привязки, а не допустимым fallback.

## Этап 4 — transform, render order и материалы

### RT-SMO-TRANSFORM

На одном простом node, одном `spStaticRenderObject` и одном skinned character:

- Position по одной оси;
- Rotation вокруг одной оси;
- uniform и nonuniform Scale;
- согласованный Transform/InvTransform static object.

Проверить world/local composition, handedness, winding/culling и сохранение
nonuniform scale после повторной загрузки.

### RT-SMO-MODEL-ORDER

На перекрывающихся объектах отдельно менять:

- `AlphaSortEnable`;
- `Priority` в обе стороны;
- `ProjectionGroup` между наблюдаемыми значениями.

Нужны кадры с фиксированной камерой, иначе порядок невозможно доказать.

### RT-SMO-MATERIAL

Выбрать по одному production-примеру для `FinalBlendOp 0/2/4/5/6`. Для каждого:

1. baseline с сохранением texture alpha и vertex diffuse;
2. одиночное изменение `FinalBlendOp`;
3. одиночное изменение каждого state slot, значение которого реально различается
   у подтверждённых opaque/transparent пар;
4. тест Z-write на двух перекрывающихся поверхностях;
5. тест alpha/overlay на глазах, рте, стекле, glow/trail.

Не перебирать все `UInt32` вслепую. Пары значений выбираются из существующего
корпуса, чтобы каждый запуск различал две реальные engine-конфигурации.

### RT-SMO-CONTROLLER

- у `spUVController` менять FunctionType только на другой наблюдаемый type и
  записывать траекторию UV во времени;
- у `spMaterialColorController` менять одну константу/амплитуду и сравнивать
  итоговый material color;
- у `spAnimTexController` менять последний timestamp и наблюдать loop/clamp.

## Этап 5 — collision, BV и navigation

### RT-SMO-COLLISION-GROUP

На небольшом статическом collider переключить Group `1 <-> 2`, затем проверить:

- движение персонажа;
- projectile/interaction, если доступно;
- camera collision;
- debug collision view;
- звук/particle поверхности.

### RT-SMO-FACE-DATA

Для одного треугольника/небольшой поверхности отдельно изменить:

- surface type на другой подтверждённый тип;
- один бит flags;
- surface ID.

Сначала подтвердить ожидаемый surface sound/effect для изменения type. Только
после этого отрицательный результат flags/ID имеет ценность.

### RT-SMO-BV

- изменить radius SphereBV и size BoxBV/OBB;
- проверить culling, collision и debug draw раздельно;
- затем материализовать explicit default Position/Rotation и сравнить с omitted
  field, если primitive structural mutation уже доказана.

### RT-SMO-NAVIGATION

На test world или небольшом уровне:

- переключить `Enabled` navigation set;
- закрыть один `ZonePortal.Open`;
- изменить один alternative reserved byte;
- наблюдать path выбора несколькими NPC/персонажами и переход между зонами.

Routing matrices и authored links не перестраиваются на этом этапе.

### RT-SMO-OCCLUSION

Переместить существующий volume через node transform так, чтобы камера по очереди
оказалась по обе стороны. Сравнить render/culling с baseline и отключённым
volume. Геометрию volume не перестраивать.

## Этап 6 — fog, light, particles и специализированный render

### RT-SMO-FOG

На сцене с фиксированной камерой проверить:

- alpha-байт цвета `0/255` при одинаковом RGB;
- type `0/1/2/3`;
- ненулевую density;
- перестановку start/end как отрицательную границу.

### RT-SMO-LIGHT

- point -> spot с существующими hotspot/falloff fields;
- изменение intensity/range;
- Enabled toggle;
- project-shadow/attenuation только во второй волне, поскольку эти optional
  fields отсутствуют в корпусе.

### RT-SMO-PARTICLE

По одному менять lifetime, emission rate, один range pair, loop, world-space и
iterative. Для каждого теста записывать первые секунды, steady state и момент
остановки; один screenshot недостаточен.

### RT-SMO-LENS-SKY

- у LensFlare менять существующие `occlusionSphereRadius`/`occlusionSpeed` и
  пройти источником за occluder;
- у SkyBox проверить camera-follow перемещением/поворотом камеры;
- ненулевой compound glare, новые elements и reorder inline sky models не входят
  в быстрый этап: они требуют structural relationship rewrite.

## Этап 7 — GUI и текст

### RT-SMO-GUI-ANCHOR

В `igmenu_opt_pc.smo` отдельно изменить Position узлов `resolution`,
`value_resolution`, `resolution_label` и `GUICollision`. Определить, что следует
за anchor: текст, визуальный state, hit target или всё вместе.

### RT-SMO-TEXT

В `menu.smo`:

- equal-length UTF-16LE замена ASCII-текста;
- glyph remap для одного безопасного символа;
- байты `0x80..0xFF` для определения code page;
- цвет и transform текста.

`wrap_width` и `alignment` добавлять только после успешного теста вставки простого
optional поля. Проверить visual bounds и hit testing отдельно.

### RT-SMO-RUNTIME-TEXT

В `gameover.smo` и других anchor-only scenes выполнить rename и transform узлов,
чтобы доказать связь с создаваемым runtime текстом. Отсутствие TextNode в SMO не
следует трактовать как отсутствие текстовой подсистемы.

## Этап 8 — вторая волна structural defaults

Начинать только если несколько same-size тестов стабильны, а текущий mutation
engine создаёт byte-accounting report без расхождений.

Порядок от простого к сложному:

1. добавить/удалить один scalar optional field в маленьком leaf object;
2. материализовать BV Position/Rotation с constructor default;
3. добавить Light project-shadow/attenuation;
4. добавить Text wrap/alignment;
5. добавить billboard axis простому node.

После каждого пункта нужны strict parse, native load, baseline restore и повтор.
Любой crash останавливает structural-волну для этого класса до выяснения причины.
Relationships, object count и inline subtree на этом этапе не изменяются.

## Порядок выполнения

1. Подготовить inventory/debug-menu карту и baseline sweep.
2. Закрыть header и animation name binding — это главные источники грубых ошибок.
3. Проверить transform, model order и реальные material states.
4. Проверить collision/BV/navigation на уровнях.
5. Проверить controllers, fog/light/particles/lens/sky.
6. Проверить GUI anchors и текст.
7. Только затем перейти к простым optional fields.
8. Повторить общие fixed-size контракты на PS2 там, где emulator/debug path даёт
   наблюдаемый результат без отдельной разработки инструментария.

## Результат каждой волны

После серии тестов должны обновляться одновременно:

- соответствующий `docs/research/smo-class-*.md`;
- [`../../research/open-questions.md`](../../research/open-questions.md);
- evidence/notes в `smo-corpus-v2.sqlite`;
- журнал с отрицательными результатами;
- regression fixture, если тест уточнил decoder или validator;
- Viewer only тогда, когда runtime-поведение подтверждено, а не просто выглядит
  правдоподобно.

Этап считается завершённым, когда выполнен baseline sweep и все P0/P1 вопросы
либо закрыты, либо имеют конкретный воспроизводимый blocker. Ненаблюдаемые
сложные serializer-ветви и полноценная structural запись не удерживают этот
рубеж: они остаются явно отмеченным backlog разработки.
