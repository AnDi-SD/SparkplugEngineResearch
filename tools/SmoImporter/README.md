# SmoImporter

Следующая версия: **0.2.0** (в разработке).

Первый безопасный importer изменяет только vertex records существующего `spMeshData`
и всегда пишет новый SMO. GUI загружает исходный SMO как серый фон, OBJ/GLB как
оранжевую модель замены и позволяет настроить uniform scale, XYZ rotation и translation.

В `0.2.0` добавлен отдельный режим **SMO → SMO**. Если модель-донор уже является
SMO, GUI отключает подгонку, rigid bone, ручную текстуру и повторную нарезку.
Importer проверяет платформу и совместимость общих костей, после чего полностью
сохраняет полный object graph target: каталог, IDs, skeleton, collision volumes,
movement trackers, `SubMaster`, IK/control nodes, attachments, character markers
и ещё не расшифрованные связи. Меняются только существующие mesh/texture leaf
objects и reference-only skin palettes; размеры и offsets контейнера пересчитываются.

Этот консервативный режим подтверждён рабочими переносами Diaspro, Nessa, Icy и
Spirit. Если donor требует больше texture groups, чем есть у target (например,
Faragonda: `3 → 2`), сохранение блокируется. Эксперименты с объединением atlas и
полной заменой render graph прошли strict parser, но вызвали crash игры и удалены.
Безопасное добавление новых visual branches требует дальнейшего исследования всех
object references; importer больше не выдаёт такой файл как готовый результат.

Различающийся набор костей показывается в раскрываемом дереве GUI. Совпавшие
bone names используются для перепривязки служебных ссылок. Дополнительные weighted
кости донора показываются в дереве как игнорируемые; их weights сворачиваются в
ближайшие совместимые target bones, поэтому их отдельная локальная анимация теряется.

Иерархия сравнивается по ближайшему weighted-предку. Различающиеся невзвешенные
control/helper nodes не блокируют совместимую deform-цепочку, но показываются
отдельной веткой с полными target/donor paths. Для `Spirit.smo` так распознаются
шесть цепочек через `C-LegRoot*`, `C-ArmRoot*` и `C-SpineRoot*`; её
`L/R_strip_1…3` сворачиваются в соответствующие `L/R_UpperArm`.

Различающиеся подтверждённые vertex layouts конвертируются field-wise в layout
target slot: position, normal, UV0/UV1, diffuse color, weights и indices. Для
`0x093E → 0x097E` отсутствующие normals вычисляются по triangles.

В `0.2.0` также появился экспериментальный путь **skinned GLB → SMO** для уже
подготовленной внешней модели. Он читает `JOINTS_0`, `WEIGHTS_0`, inverse bind
matrices, материалы и embedded base-color, показывает дерево соответствий костей
и сохраняет весь object graph целевого SMO. Вершины автоматически переводятся из
bind pose GLB в bind pose target, а triangles переразбиваются по существующим
16-костным palettes. Неизвестные имена костей не угадываются: без подтверждённого
соответствия запись блокируется.
Вложенные однокостные render-ветки (например, `bloom_eyes` под `Head`) считаются
жёсткими runtime-контрактами и больше не перепрофилируются под произвольный
material primitive. Несовместимый primitive объединяется с основным body group с
явным предупреждением о потере отдельных material/alpha flags.
Игровая диагностика подтвердила, что полная замена Alpha в ABGR texture leaf
вызывает crash. Поэтому skinned GLB writer переносит только RGB, а Alpha всегда
сохраняет от target SMO.

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
```

Архитектура двух следующих вариантов — модели с уже правильными костями и модели
без костей/с неверной привязкой — описана в
[`BONE_BINDING_DESIGN.md`](BONE_BINDING_DESIGN.md).

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.Gui
```

GUI умеет объединить все meshes входной OBJ/GLB-сцены и детерминированно
разрезать triangle stream по настраиваемым `MaxVertices`/`MaxTriangles`.
Каждый chunk получает локальную таблицу индексов; вершины на границах chunks
дублируются. Интерфейс сравнивает число chunks с количеством существующих
`spMeshData` slots исходного SMO.

Текущий экспериментальный **unskinned** OBJ/GLB whole-model режим:

- объединяет meshes OBJ/GLB в один rigid body-slot с новой topology;
- оставляет остальные существующие render slots валидными через degenerate triangle `(0,0,0)`;
- записывает всем вершинам rigid weight `(1,0,0,0)` на выбранный подтверждённый palette slot;
- принимает embedded GLB base-color либо внешний PNG/JPEG;
- безопасно заменяет только RGB существующего ABGR atlas, сохраняя исходный Alpha, headers, offsets и длину SMO;
- никогда не перезаписывает исходный SMO.

В OBJ/GLB-режиме изменение разрешения texture atlas и texture repack намеренно запрещены: старый
алгоритм `SMOTextureTool` создавал структурно читаемые файлы, которые вызывали
вылет оригинальной игры.

Проверка безопасного export/import round-trip:

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -- path/to/model.smo
```
