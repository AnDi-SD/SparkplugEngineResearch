# Occlusion Init/reader/world в общем ядре

Общий ResourceGraph теперь загружает pristine `Levels/Challenges/race_02.smo`
целиком: **7513 объектов, 319 Node, два spOcclusionVolume**. Native acceptance
создала сцену, обновила позу и настроила LightManager; итоговый прогон занял
0,208 s. Исходный файл не изменялся. Это проверка общего ядра приложения, не
запуск всей игры или проверка GPU изображения.

Managed `SmoLoadedResources → SparkplugSceneRuntime → SmoSceneBuilder` также прошёл
на этом уровне: 319 Node, 2075 supports/2212 members, 960 проверок; исходные Node
world bits одинаковы в snapshot и live runtime. Ошибок построения сцены нет. Это
проверка настоящего потребителя C# поверх общего графа.

В восстановленный `spOcclusionVolume` перенесены fresh PC Init `470FE0` и world
update `46DD90`; новый `spOcclusionVolumeSerializer` (`7F3030B8`) читает inherited
Node и собственные IB/VB секции, вызывает Init и освобождает временные inputs.
Serializer зарегистрирован для PC mask6 с явной [общей CRT policy](tool-shared-sort-policy-2026-09-11.md).
Приложения используют один общий loader. Дополнен настоящий `spNode::TransformPoint`
(`420660`), включая отдельный порядок округления/сложения по координатам.

Сохранены особенности оригинала: low16 при UInt32 IB; position-only deep copy;
weld до topology; world positions в отдельном owned storage; сфера и bounds по
**исходному входному VB**, включая удалённые compactor вершины; min/max начинаются
с нуля. Shape failure сохраняет уже построенное промежуточное состояние. Node-only
clone не копирует собственную геометрию. World update меняет живые точки, отмечает
camera dirty и вычисляет отдельную world sphere; Enabled не добавлен как gate.

Один original guest проверил пять свежих объектов: authored quad, положительный
quad с высокими16 bits индексов, duplicate5, удаляемую неиспользуемую вершину и
полный reader authored объекта. Для каждого выполнен original world с translation,
отрицательным/неравномерным scale и поворотом. Совпали **1330 reference words**:
IB/VB, bounds/spheres, plane bits, face/edge identity и порядок outgoing lists.
Один лишь подсчёт faces/edges сравнением не считался.

Пакет использовал character4M/16s, процесс30s, один worker и явно выделенный
128KiB guest arena; фактически bump usage62496 bytes. Первая защищённая подготовка
заняла 10,684 s, следующие Init/reader — 0,049–0,117 s, весь пакет12,717 s.
Все tracked allocations освобождены. Ошибка первой версии проверочного скрипта
в аргументе `cleanup` сохранена как run1; исправлен только harness, новый run2
прошёл на свежем guest. Продолжения остановленного guest не было.

NodeWorld/FullLoader/OcclusionTopology/OcclusionRuntime suites прошли 4/4. После
защиты live runtime от замены внутреннего storage через analysis setters две
затронутые suites повторены и прошли. Прежнее предупреждение C4756 в общем
`spVertexBounds.h` сохраняется; сборка не объявляется свободной от предупреждений.
[Evidence и точные snapshots](../../research/tools-core-occlusion-runtime-2026-09-11.json).

Границы: повторный Init, одиночный triangle full shape и повторные IB/VB fields
остаются явными host refusals из-за незакрытого оригинального lifetime. Для новой
геометрии создаётся новый owner. Writer/serializer clone, camera silhouette,
Scene partition notifications и PS2 Init не объявлены перенесёнными. Прежний
`SmoOcclusionVolumeDecoder` остаётся отдельно обозначенным строгим инспектором
корпуса/полей, использующим общие CPU buffer readers; он не подменяет этот runtime.
