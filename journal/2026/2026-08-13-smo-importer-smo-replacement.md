# 2026-08-13 — SmoImporter: visual transplant SMO → SMO

## Уточнённая задача

Целевой игровой ресурс нельзя заменять файлом донора целиком. Например,
`Bloom/bloom_jeans.smo` содержит collision, movement trackers, SubMaster,
attachments и другие данные, которых нет в `Diaspro/Diaspro.smo` и которые может
проверять игра. Поэтому результат должен сохранить внутреннюю структуру target,
заменив внутри неё только меши и текстуры визуальной моделью донора.

## Подтверждённая структура контрольной пары

- `bloom_jeans.smo`: 121 объект каталога, 6 skinned meshes и 2 textures;
- `Diaspro.smo`: 105 объектов каталога, 6 skinned meshes и 2 textures;
- у моделей совпадают 52 bone names и их иерархия, но различаются все 52 bind poses;
- оба body набора разбиты на пять `spSkin`/`spMeshData` slots с 16-костными
  hardware palettes, глаза находятся в шестом slot;
- body texture имеет размер `256×256`, eye texture — `64×64`.

## Реализация

`SmoVisualTransplanter` использует target как контейнер и сохраняет его object
directory, IDs, names, types, node graph, materials, collision, attachments и
остальные неизвестные объекты. Для visual payload выполняются отдельные операции:

1. Полные serialized `spTextureData` донора, включая Alpha, сопоставляются по
   размерам и записываются в target texture slots. Target material references и
   имена texture objects остаются прежними.
2. Raw vertex records и triangles донора извлекаются вместе с bone names,
   полученными через его skin palettes.
3. Triangle bone sets детерминированно распределяются по существующим target skin
   slots. В каждом slot остаётся не более 16 костей; вершины дублируются только
   между новыми palette groups.
4. Packed blend indices перенумеровываются в локальные target palette indices по
   точным bone names. Inverse bind matrices берутся у донора; node references,
   skeleton graph и анимационные узлы остаются от target.
5. Каталог логических offsets/sizes, размеры всех содержащих объектов,
   `FileSize`/`DataSize` и inline reference sizes пересчитываются после repack.

Подтверждённой общей checksum внутри SMO не найдено. SHA-256 результата является
внешним идентификатором, а не полем формата.

## Ограничения режима

Текущая версия требует одинаковые serializer/platform, совместимую иерархию
общих bones, безопасный fallback для дополнительных donor bones, наличие skinned
palettes в обеих моделях, достаточное число target mesh groups и однозначное
сопоставление texture groups. При нехватке texture slots поддерживается объединение
подтверждённых совместимых ABGR-групп `0x32E3`/`0x43E3`; другие форматы блокируются. Число donor
meshes/palettes может отличаться: triangles перераспределяются по target slots, а
лишние target slots сохраняются с degenerate triangle. Разные bind poses допускаются с
предупреждением, поскольку inverse bind matrices донора сочетаются с target node
graph; результат обязательно нужно проверить в игре. Более общие пары относятся
к будущему полноценному repacker/режиму перепривязки.

## Проверка

Для `bloom_jeans.smo` + `Diaspro.smo` создан
`artifacts/test-output/bloom_jeans_from_diaspro.smo`:

- результат содержит все 121 target object identity, тогда как донор содержит 105;
- collision, movement trackers, SubMaster и attachments target присутствуют;
- перенесены 6 meshes, 1492 triangles и 2 texture pixel buffers донора;
- body Alpha содержит 537 прозрачных пикселей, eye Alpha — 1;
- все 6 `spSkin` и mesh bindings проходят строгую декодировку;
- `SmoViewer.FormatTests`: `PASS: 263 assertions`;
- target и donor не изменены; output не совпадает побайтно ни с одним из них.

Дополнительная пара `bloom_jeans.smo` + `Nessa.smo` подтвердила работу с разной
нарезкой:

- target: 121 объект, 6 mesh slots и 2 texture slots;
- donor: 97 объектов, 5 meshes, 5 отличающихся palettes и 1 texture;
- все 1256 реальных triangles Nessa распределены по пяти body-slots Bloom;
- отдельный eye-slot Bloom сохранён с одним невидимым `(0,0,0)` triangle, его
  texture object сохранил target ID/ссылки, но получил копию texture data Nessa;
- единственный `256×256` atlas Nessa перенесён вместе с Alpha во все два target
  texture slots; старые пиксели Bloom в output отсутствуют;
- `SmoViewer.FormatTests`: `PASS: 263 assertions`;
- `SmoExporter.FormatTests`: `PASS: 14 assertions`.

Пара `bloom_jeans.smo` + `Icy.smo` добавила частично различающийся skeleton:

- 52 основные weighted bones совпали по имени;
- дополнительные `Hair_01`, `Hair_02`, `Hair_03`, `Hair_04` отсутствуют в Bloom
  и свёрнуты в `Head`;
- дополнительные `Cape_01`, `Cape_02` свёрнуты в `Pelvis`;
- самостоятельная анимация этих шести костей намеренно теряется, но их vertex
  weights не становятся dangling references;
- 12 meshes и 1248 triangles Icy распределены по 5 body-slots Bloom;
- 1 texture Icy записана в оба target texture slots при сохранении их IDs;
- output сохраняет 121 target object identity против 119 объектов Icy;
- `SmoViewer.FormatTests`: `PASS: 263 assertions`;
- `SmoExporter.FormatTests`: `PASS: 14 assertions`;
- SHA-256 результата:
  `0395A8438F25F5F98D883A480D28CB723328FDAF9522C89EC4D36996B0C622AE`.

В GUI добавлено раскрываемое дерево, сформированное тем же mapping planner:
совпавшие кости, дополнительные donor bones с точным fallback и target bones без
donor weights. Последняя группа не блокирует запись, но подсвечивается как риск
неправильной деформации. Если дополнительной donor bone нельзя однозначно найти
общего weighted предка или ближайшую shared bone по bind position, writer
блокирует сохранение.

Пара `bloom_jeans.smo` + `Spirit.smo` потребовала одновременно адаптации
иерархии и vertex layout:

- все 52 основные weighted bones совпадают по имени и ближайшему weighted parent;
- у `L_Thigh`, `R_Thigh`, `Pelvis`, `Spine_01`, `L_Clavicle`, `R_Clavicle`
  различаются только промежуточные control nodes (`C-LegRoot*`, `C-SpineRoot*`,
  `C-ArmRoot*`); эти пути показаны в отдельной ветке GUI;
- дополнительные `L_strip_1…3` свёрнуты в `L_UpperArm`, `R_strip_1…3` — в
  `R_UpperArm`; их самостоятельная анимация теряется, weights сохраняются;
- 10 meshes и 1244 triangles Spirit распределены по target slots Bloom;
- compact skinned layout Spirit `0x093E` со stride 44 преобразован в Bloom
  `0x097E` со stride 56; normals вычислены по triangle topology;
- совпавшие после remap influences суммированы и нормализованы;
- output сохраняет 121 target object identity против 122 объектов Spirit;
- `SmoViewer.FormatTests`: `PASS: 263 assertions`;
- `SmoExporter.FormatTests`: `PASS: 14 assertions`;
- SHA-256 результата:
  `2B34FB40B197F78DA9B241773BE54C09D10648878130E94D722937E4A9B8CAA0`.

Пара `bloom_jeans.smo` + `Faragonda.smo` выявила отдельную причину прежнего
отказа: совместимы все 52 weighted bones, но у донора 7 meshes/3 texture groups,
а target предоставляет 6 mesh/2 texture slots. Реализован texture-atlas fallback:

- `farago_e` и `farago_g` (`64×64`, ABGR `0x32E3`) объединены горизонтально в
  один `128×64` atlas с автоматическим масштабом/смещением U;
- основная `faragond` `256×256` сохранена отдельной группой;
- перенесены все 7 meshes и 1440 невыражденных triangles; пиксели и Alpha всех
  трёх donor textures подтверждены через histogram проверку результата;
- output сохраняет все 121 object identity Bloom против 110 объектов Faragonda;
- `SmoViewer.FormatTests`: `PASS: 263 assertions`;
- `SmoExporter.FormatTests`: `PASS: 14 assertions`;
- SHA-256 результата:
  `E0326F56169A80ECA8FFF9524F3883E4DEDDD5A11482577262611DDDDF6DA030`.

Релизная сборка и визуальный/игровой тест не выполнялись по ранее заданному
ограничению.

## Коррекция архитектуры после игрового теста Faragonda

Первый Faragonda-output `E0326F…` был структурно валиден, но игра завершилась с
ошибкой. Этот результат и вывод о безопасном atlas merge считаются опровергнутыми.
Причина подхода: writer ошибочно рассматривал 6 mesh/2 texture slots Bloom как
обязательный контейнер и пытался разместить в нём штатные 7 mesh/3 texture groups
Faragonda. Даже полное обновление известных FFPS/catalog sizes не делает такое
изменение игрово безопасным.

Первой реакцией был graph transplant:

- visual subtree target полностью исключается вместе с meshes, `spSkin` palettes,
  materials, textures и вложенными render nodes;
- все верхнеуровневые render roots донора переносятся целиком и без изменения
  object IDs, mesh bytes, vertex layouts, UV, palettes, textures или Alpha;
- очищенный `model_root_master` target сохраняет collision volumes, movement
  trackers, `SubMaster`, IK/control и character marker `BLOOM`;
- target service objects получают новые IDs после диапазона donor visual IDs;
  reference-only связи `C-lowerRoot → Pelvis` и `C-upperRoot → Spine_01`
  перепривязаны к одноимённым donor nodes;
- root, object directory, logical offsets, inline sizes, `FileSize` и `DataSize`
  строятся заново.

Новый `bloom_jeans_from_faragonda_graph.smo` содержит 117 объектов, исходные
7 meshes, 7 palettes, 3 textures, UV controller и 1440 triangles Faragonda, а
также служебную ветвь Bloom. Geometry/UV/texture fingerprints побайтно совпадают
с донором; Viewer проходит 250 assertions, Exporter — 14. SHA-256:
`9C8367AD2BEB86513E670373C1F51851786F9848C543810502CF084303F9250C`.

Последующий игровой тест показал, что этот writer также аварийный и ломает даже
ранее работавшие пары. Причина: перенос только выделенного service graph не
сохраняет все неизвестные target bindings. В частности, скан обнаружил ссылки
`SubMaster`/`UpperBody` на `Pelvis` и `Spine_01`, а также `C-lowerRoot` и
`C-upperRoot`; известными field-5 ссылками граф не исчерпывается.

Graph transplant удалён. Активный writer возвращён к последнему совместимому
состоянию: сохраняет число, порядок, IDs, имена, типы и parent topology всех target
objects и меняет только существующие visual leaves/reference-only palettes.
Faragonda (`3` texture groups против `2`) теперь блокируется до записи.

## Первый подготовленный skinned GLB: `uzhs.glb`

Для внешней модели `local-data/uzhs.glb`, подготовленной на скелете Bloom,
реализован отдельный экспериментальный путь. GLB содержит один skin с 55 joints,
два material primitives, 560 исходных vertices и 300 triangles. Из 37 joints с
реальными weights 34 точно совпадают с deform-костями `bloom_jeans.smo`.
Три служебных joints перенаправлены явно:

- `C-lowerRoot → Pelvis`;
- `C-upperRoot → Spine_01`;
- `neutral_bone → Pelvis`.

У всех 37 активных joints bind pose отличается от target. Writer нормализует
weights и выполняет линейный rebase каждой позиции/нормали через
`donorInverseBind × targetBindWorld`, после чего заново разрезает triangles по
существующим шести target palettes с лимитом 16 костей. Target graph, IDs,
skeleton, collision и неизвестные связи не меняются. Два primitives сопоставлены
с двумя существующими texture groups, embedded RGBA записана в фиксированные
ABGR leaves без изменения их размера.

В GUI для skinned GLB остаётся дерево привязки костей; rigid bone и обычная
подгонка скрываются, а автоматический rebase включён по умолчанию. Произвольный
fallback по ближайшей позиции запрещён: новое несовпавшее active joint блокирует
запись до появления ручного редактора соответствий.

Создан `artifacts/test-output/bloom_jeans_from_uzhs.smo`:

- 6 output mesh slots, 566 serialized vertices (560 исходных и 6 дубликатов на
  границах palettes), 300 triangles и 6 palettes;
- strict import test пройден, SHA-256:
  `A9566DDE9B50A2DC92C378DA75D5825BF378A2876C8FD6C969637348342768CE`;
- `SmoExporter.FormatTests`: `PASS: 14 assertions`;
- обратный GLB-export: 566 конечных positions, `nonfinite=0`, warnings `0`.

Это доказывает структурную целостность, но не игровую совместимость и не качество
позы. Релизная сборка, визуальный и игровой тест не выполнялись.

Отдельный риск находится в исходном GLB: его прозрачный primitive из 16 vertices
и 8 triangles занимает почти весь рост модели и уже в файле имеет смешанные
weights на `L_Bicep`, `R_Bicep`, `R_calf`, `R_Ankle` и `R_Middle_03`. Writer эти
данные не придумывает и не исправляет. Если именно эта деталь разъедется в игре,
следующим шагом должна быть коррекция weights в исходной модели или отдельный
ручной remap, а не изменение skeleton Bloom.

### Коррекция после игрового crash

Первый результат с SHA-256 `A9566DDE…` вызвал вылет игры. Файл, вручную помещённый
в `local-data/Winx Club`, побайтно совпал с output импортера, поэтому копирование
не было причиной. Сравнение с pristine Bloom и рабочей Diaspro-подменой выявило
нарушенный runtime-инвариант: маленький прозрачный primitive был сопоставлен с
`bloom_eyes` только по числу triangles. Исходная eye-ветка вложена под `Head`, а
все 16 её palette slots ссылаются только на `Head`; аварийный output заменил их
на `L_Bicep`, `R_Bicep`, `R_Ankle`, `R_calf` и `R_Middle_03`.

Planner исправлен:

- target group, все palette entries которого указывают на одну кость, считается
  жёсткой вложенной веткой и никогда не переписывается;
- donor material group, не помещающийся в такую ветку, объединяется с основным
  body group с явным предупреждением о потере отдельных material/alpha flags;
- `bloom_eyes` получает один невидимый degenerate triangle и сохраняет исходные
  16 ссылок на `Head`;
- все 300 реальных triangles `uzhs.glb` распределены по пяти body palettes.

Новый кандидат `artifacts/test-output/bloom_jeans_from_uzhs.smo` имеет SHA-256
`BE009736E2B8F19983FDE48BDF10CBD4CE391D7B36C806EE39BAF51F779DA8D5`, проходит
263 Viewer assertions и 14 Exporter assertions. Игровая совместимость этого
варианта пока не подтверждена.

Второй игровой тест показал, что `BE009736…` также вызывает вылет. Следовательно,
перезапись eye palette была реальной ошибкой writer, но не единственной причиной
аварии. Для дальнейшей локализации создан набор
`artifacts/test-output/uzhs-diagnostics`:

1. `01_texture_rgb_only.smo` — pristine geometry/skin/graph, меняется только RGB,
   исходный Alpha сохраняется;
2. `02_texture_rgba.smo` — pristine geometry/skin/graph, меняются RGB и Alpha;
3. `03_body_rigid_pelvis_original_texture.smo` — только основной primitive из
   292 triangles, исходные textures, все vertices жёстко привязаны к `Pelvis`,
   target palettes не перестраиваются;
4. `04_body_skinned_original_texture.smo` — те же 292 triangles и исходные
   textures, но включены GLB weights, bind-pose rebase и перестроение palettes.

Все четыре файла сохраняют 121 target object, вместе проходят 900 Viewer
assertions, каждый отдельно проходит 14 Exporter assertions. Интерпретация
последовательного игрового теста: crash на №1 указывает на RGB writer; только на
№2 — на Alpha; на №3 — на mesh topology/serialization; только на №4 — на
skinning/palette/bind conversion. До результатов этих проб основной skinned GLB
writer не считается игрово совместимым.

Результат игрового прогона диагностической матрицы:

- №1, RGB при сохранённом target Alpha — работает;
- №2, RGB вместе с donor Alpha — вылет;
- №3, новая body topology с rigid `Pelvis` — работает;
- №4, новая body topology с настоящими weights/palettes/rebase — работает.

Тем самым GLB topology, vertex serializer, palette writer и bind-pose rebase
исключены как причины этого crash. Причиной оказалась именно перезапись Alpha
существующего ABGR texture payload. Production skinned writer переключён на
`FixedSizeTextureWriter.ReplaceRgb`; исходные Alpha bytes target сохраняются.
Полная RGBA-функция переименована в `ReplaceRgbaDiagnosticUnsafe` и больше не
может быть случайно вызвана обычным GUI-путём.

Полный output пересобран с двумя primitives и сохранённым target Alpha как
`artifacts/test-output/bloom_jeans_from_uzhs.smo`, SHA-256
`EAC1925D6310B1175FFF3EBC6EAD2BC1CEE7C43F9D527A94D6B3A4911778A737`.
Вариант `BE009736…` сохранён отдельно под именем
`bloom_jeans_from_uzhs_rejected_rgba_crash.smo` только для воспроизводимости.
Новый output проходит 264 Viewer assertions и 14 Exporter assertions; итоговый
игровой тест полного сочетания ещё требуется.

## Поддержка подготовленного `uzhs.fbx`

В Importer добавлен FBX reader-адаптер через Blender. Он использует тот же
расширенный поиск Blender, что Exporter, запускает его в background mode,
импортирует FBX без анимации и leaf bones, выбирает только meshes с Armature
modifier и связанные armatures, экспортирует временный GLB с четырьмя главными
influences и активным vertex color, а после чтения удаляет временный каталог.

Это важно для `uzhs.fbx`: кроме нужных `UpperBody` и `ch_L1_C51.001`, файл
содержит отдельную 42-вершинную `Икосфера` без skinning. Она автоматически
исключена. Итог сохранил 55 joints, 37 активных, 34 точных соответствия, те же
три служебных remap, 560 исходных vertices/300 triangles и два primitives.

Создан `artifacts/test-output/bloom_jeans_from_uzhs_fbx.smo`, SHA-256
`1B14E65E94F019E6BE0CD712F6715CCB7E97F142B473DFDA476E86BED5E6249F`.
Результат проходит 264 Viewer assertions и 14 Exporter assertions. В соответствии
с подтверждённым ограничением переносится RGB, а Alpha остаётся от target SMO.
Релизная сборка, визуальный и игровой тест FBX-варианта не выполнялись.
