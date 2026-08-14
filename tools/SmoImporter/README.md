# SmoImporter

Текущая версия: **0.3.0**.

Пользовательский архив предназначен для Windows x64. Запускайте
`SmoImporter.Gui.exe` из корня распакованного пакета: загрузчик проверит наличие
Microsoft .NET 8 Desktop Runtime и, только после подтверждения, предложит скачать
официальный установщик Microsoft. Само приложение находится в `app`, документация —
в `docs`. Отдельный `SmoNativeValidator.exe` не поставляется: его ядро встроено в
Importer и используется только для автоматической проверки созданного SMO.

Importer всегда пишет новый SMO и не разрешает сохранять результат поверх target
или donor. GUI загружает исходный SMO как серый фон, OBJ/GLB/FBX как оранжевую модель
замены и позволяет настроить uniform scale, XYZ rotation и translation.

В `0.2.0` появился отдельный режим **SMO → SMO**, а в `0.3.0` его writer переведён
на append-only visual packing. Если модель-донор уже является SMO, GUI отключает
подгонку, rigid bone, ручную текстуру и повторную нарезку. Importer проверяет
платформу и совместимость общих костей, сохраняет исходный
service/skeleton graph target: его каталог, IDs, collision volumes, movement
trackers, `SubMaster`, IK/control nodes, attachments, character markers и ещё не
расшифрованные связи. Старые target meshes остаются структурными anchors, но получают
невидимые degenerate triangles. Полные donor visual branches — `spSkin`, materials,
textures, UV controllers и meshes — добавляются отдельно с новыми уникальными IDs.
Donor nodes и служебный graph не копируются: reference-only palettes указывают на
target nodes по именам костей и сохраняют donor inverse-bind matrices. После этого
пересчитываются каталог, enclosing sizes и offsets контейнера.

Такой append-only путь не пытается сжимать donor atlases в существующие target slots
и не заменяет неизвестный runtime graph целиком. Контрольный перенос Faragonda в
`bloom_jeans.smo` сохранил все 121 target objects, добавил 21 visual object,
7 meshes и 3 исходные donor textures. Один byte-identical общий visual helper
переиспользуется, поэтому итоговый каталог содержит 142 objects. Результат прошёл strict parser, быстрый
native smoke test и контекстную загрузку в настоящем Bloom slot на Gardenia02.

Упаковщик также поддерживает visual forest из нескольких render roots. Контрольные
StellaX ↔ `bloom_jeans.smo` сохраняют отдельную голову, rigid-крылья, все 38 ссылок
анимированной texture sequence и общий атлас `stella_x`. Оба направления прошли
strict и быстрый native smoke test; StellaX → Bloom дополнительно проверен в
настоящем Bloom slot.

Для rigid GLB, OBJ и FBX без скелета добавлен отдельный **multi-texture → SMO** путь. Он
связывает mesh/material с PNG по именам `matN.png` либо `_matN.png` и кадрам
`matN.1.png`, `matN.2.png` и т. д., создаёт отдельные `spSkin`, material,
texture и mesh branches для каждой material group и сохраняет полный BGRA, включая
Alpha. Уже кратные степени двойки текстуры переносятся пиксель-в-пиксель; остальные
увеличиваются по каждой оси до ближайшей следующей степени двойки, никогда не
уменьшаются и ограничены размером 2048. Геометрия жёстко привязывается к palette
slot 8, который target обязан разрешать как `Head`; это номер слота конкретной
палитры, а не глобальный ID кости.

GUI автоматически проверяет PNG рядом с моделью, но также позволяет выбрать отдельную
папку с текстурами. OBJ разбирается по `mtllib`/`usemtl`/`map_Kd`; если MTL отсутствует,
достаточно material-имён `matN` и выбранной папки. FBX конвертируется через Blender,
после чего geometry primitives связываются по material/image metadata. Meshes и PNG,
которые не относятся ни к одной активной `matN`-группе, явно показываются как пропущенные.

Контрольные импорты обновлённых `local-data/Лейла/Model.obj` и `Model.fbx` в
`bloom_jeans.smo` сохранили 7 material meshes, 11 исходных textures и две texture sequences
для групп `mat3`/`mat4`. Все 11 PNG этого набора уже имеют размеры степени двойки,
поэтому их RGBA не масштабировался. Результат прошёл strict-проверку, быстрый native
smoke test и контекстную загрузку в настоящем Bloom slot; все нативные проверки
запускались только в изолированном оконном режиме. Порядок дополнительных
кадров выведен из соглашения имён и нативного texture-sequence шаблона; его всё ещё
следует визуально проверить в игре.

После каждого успешного сохранения GUI автоматически запускает короткую нативную
проверку в изолированном оконном окружении. Пользователю остаётся только путь к
`WinxClub.exe`; итог показывается одной карточкой «подходит / не подходит / не
определено» без журнала и дополнительных настроек. Зелёный результат подтверждает
FFPS header/version, ненулевой `ResourceLoad` и окно наблюдения, но не заменяет
визуальную проверку анимаций и поведения модели в игровом уровне.

Различающийся набор костей показывается в раскрываемом дереве GUI. Совпавшие
bone names используются для перепривязки служебных ссылок. Дополнительные weighted
кости донора показываются в дереве как игнорируемые; их weights сворачиваются в
ближайшие совместимые target bones, поэтому их отдельная локальная анимация теряется.

Иерархия сравнивается по ближайшему weighted-предку. Различающиеся невзвешенные
control/helper nodes не блокируют совместимую deform-цепочку, но показываются
отдельной веткой с полными target/donor paths. Для `Spirit.smo` так распознаются
шесть цепочек через `C-LegRoot*`, `C-ArmRoot*` и `C-SpineRoot*`; её
`L/R_strip_1…3` сворачиваются в соответствующие `L/R_UpperArm`.

В режиме SMO → SMO donor vertex records и topology переносятся без перекодирования.
Field-wise конвертация в target layout (`0x093E → 0x097E`, включая вычисление
normals) остаётся частью отдельного пути внешнего skinned GLB → SMO.

В `0.2.0` также появился экспериментальный путь **skinned GLB → SMO** для уже
подготовленной внешней модели. Он читает `JOINTS_0`, `WEIGHTS_0`, inverse bind
matrices, материалы и embedded base-color, показывает дерево соответствий костей
и сохраняет весь object graph целевого SMO. Вершины автоматически переводятся из
bind pose GLB в bind pose target, а triangles переразбиваются по существующим
16-костным palettes. Неизвестные имена костей не угадываются: без подтверждённого
соответствия запись блокируется.

Bind-pose перенос теперь имеет два явных режима. GUI по умолчанию использует
`RetargetToGameBindPose`: вершины и normals ровно один раз переводятся формулой
`donor inverse-bind × target bind-world`, после чего в SMO записываются только
игровые bone IDs и игровые inverse-bind matrices. Donor node hierarchy, rest-pose,
control nodes и animation logic никогда не сериализуются. Диагностический
`PreservePreparedGeometry` не меняет positions/UV/weights и допустим только с
identity transform; он предназначен для моделей, уже подготовленных точно в
игровой bind pose. GUI блокирует AutoFit и произвольный transform для skinned-моделей.

Если несколько material primitives используют один image, они остаются одной
body-atlas группой. Texture writer меняет RGB только сопоставленного texture object;
непарные eyes/effects проверяются побайтово. Контрольная подготовленная Текна
(`local-data/Текна/untitled.glb`) использует 17 точно совпавших active joints и
382 triangles. В результате сохранены обычный `Tecna.smo` skeleton/animation graph
и глазной `tecna_e`, а новый body atlas применён только к `tecna`. Файл прошёл
strict parser, быстрый native smoke и две загрузки в настоящем Cloud01_02 без
ошибок serializer и crash; все игровые проверки выполнялись оконно.
Вложенные однокостные render-ветки (например, `bloom_eyes` под `Head`) считаются
жёсткими runtime-контрактами и больше не перепрофилируются под произвольный
material primitive. Несовместимый primitive объединяется с основным body group с
явным предупреждением о потере отдельных material/alpha flags.
Skinned GLB writer в production-режиме переносит только RGB, а Alpha сохраняет
от target SMO. Прежний диагностический RGBA-crash оказался не запретом Alpha:
writer принимал служебный marker `00` на `+0x3C` за первый пиксель. Для
`0x32E3`/`0x43E3` настоящий BGRA payload начинается с `+0x3D`; это исправлено,
а исправленный full-BGRA контроль уже прошёл native load без исключения. Перенос
donor Alpha пока остаётся диагностическим до более широкой визуальной/gameplay-
проверки; production сохраняет Alpha target.

Skinned FBX использует тот же pipeline через Blender: Importer находит
`blender.exe` через ручной путь, `BLENDER_PATH`, `PATH`, реестр и стандартные
каталоги, преобразует во временный GLB только meshes с Armature modifier и сами
armatures, затем удаляет временный каталог. Это исключает случайные helper meshes
без skinning и не создаёт отдельный FBX writer с другими правилами костей.

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -- `
  --skinned-glb target.smo donor.glb output.smo

dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -- `
  --skinned-fbx target.smo donor.fbx output.smo
```

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -- `
  --smo-to-smo target.smo donor.smo output.smo

dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -- `
  --rigid-multitexture target.smo donor.obj output.smo
```

Архитектура двух следующих вариантов — модели с уже правильными костями и модели
без костей/с неверной привязкой — описана в
[`BONE_BINDING_DESIGN.md`](BONE_BINDING_DESIGN.md).

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.Gui
```

Для внешней OBJ/GLB-сцены, которая не распознана как описанный выше
`matN` multi-material bundle, GUI сохраняет старый single-atlas fallback. Он
объединяет meshes и детерминированно разрезает triangle stream по настраиваемым
`MaxVertices`/`MaxTriangles`: каждый chunk получает локальную таблицу индексов,
а вершины на границах дублируются. Интерфейс сравнивает число chunks с количеством
существующих `spMeshData` slots исходного SMO.

Этот legacy **unskinned** OBJ/GLB whole-model режим:

- объединяет meshes OBJ/GLB в один rigid body-slot с новой topology;
- оставляет остальные существующие render slots валидными через degenerate triangle `(0,0,0)`;
- записывает всем вершинам rigid weight `(1,0,0,0)` на выбранный подтверждённый palette slot;
- принимает embedded GLB base-color либо внешний PNG/JPEG;
- безопасно заменяет только RGB существующего BGRA atlas, сохраняя исходный Alpha, marker, headers, offsets и длину SMO;
- никогда не перезаписывает исходный SMO.

Ограничение на изменение разрешения относится именно к legacy single-atlas
режиму: он не перестраивает texture object и меняет только RGB существующего
атласа. Новый multi-material packer выше создаёт собственные подтверждённые BGRA
branches и может только увеличивать non-POT входы до следующей степени двойки.
Произвольный repack существующих textures алгоритмом `SMOTextureTool` по-прежнему
не используется: старый путь создавал структурно читаемые файлы, вызывавшие вылет
оригинальной игры.

Проверка безопасного export/import round-trip:

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -- path/to/model.smo
```
