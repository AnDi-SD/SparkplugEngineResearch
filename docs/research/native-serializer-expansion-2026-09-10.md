# Дополнение независимого учёта сериализаторов PC/PS2

10 сентября2026. [Машинный контракт](../../research/serializer-expansion-contracts-2026-09-10.json)
фиксирует34 имени классов, для которых отсутствовала хотя бы одна платформенная
оценка:20 PC и29 PS2. Прежние13 PC assessments не перепроверяются и не меняются.
Платформенные имена остаются раздельными: пять PC-only классов и один PS2-only.
Применяются pristine hashes, записанные в контракте; никаких driver/OS calls,
обхода всего asset corpus или сборки UI не выполнялось.

## Какие группы добавлены

Все имена ниже имеют префикс `sp` и суффикс `Serializer`.

| Группа | Новые PC строки |
| --- | --- |
| Разбиение пространства и зоны | BSPNode,OctreeNode,PartitionNode,PartitionRenderable,PCPartitionRenderable,PartitionSystem,Zone,ZonePortal,ZonePortalNode |
| Проекции | Projection,BoxProjection,PyramidProjection |
| Геометрия/коллизии | ConvexBV,CollisionInfo |
| Сцены/эффекты | Cinematic,LensFlare |
| PC графические ресурсы | DXCubeTexture,DXShaderEffect,DXShadowMesh,DXShadowVolume |

PS2 дополнительно получает самостоятельные строки Animation,Font,MeshBV,
MeshNavigationSet,NavigationGraph,NavigationPortal,NavigationSet,
OcclusionVolume,ParticleSystem,Skin,StaticRenderObject,TextNode,TextRenderable:
их существующие PC исследования автоматически на PS2 не переносятся.
PS2PartitionRenderable записан отдельно от PCPartitionRenderable.
Четыре перечисленных DX serializers присутствуют только на PC.

## PC: двадцать полных concrete lifetimes

Все20 factory/getter/clone/delete-clone/delete-original прошли:
100 class operations. Каждый новый PC объект имеет allocation20 байт,
primary vptr0 и secondary vptr10. Copy slot всех20 — существующий
no-payload40ECE0. Clone создаёт **отдельный concrete serializer**, регистрирует
пару и использует этот copy. Это не клонирование целевого графа объектов.
Остаточные allocations общего окружения записаны отдельно; полного shutdown
всех singleton’ов результат не утверждает.

Projection pilot0,456 с; остальные19 —8,467 с тремя workers.
Первый pilot остановился до Clone из-за неверного ожидания стенда:
null Clone базового spSerializer не описывает override конкретного класса.
Сохранённое тело468760 явно вызывает concrete factory4686F0, register412F70,
затем virtual copy. Изменение стенда под общий null было удалено; успешный
run использует обычный original distinct-object путь. Игра не менялась.

Для CollisionInfoSerializer дополнительно просмотрено прежнее
[PC payload evidence](native-pc-collision-tools-core-2026-09-08.md):
reader438A80,index438A40,writer438E20, исходная group/transform запись,
quaternion conversion и настоящий FFPS whole-load89 984 instructions,
31/31 frees. Эти сведения существовали до нынешнего цикла. Его первая PC
оценка65 обозначена `reviewed_existing`; это не65 пунктов нового реверса.

## PS2:29 независимых construction identities

28 factories встраивают собственные primary/secondary vtable stores после
вызова родительского constructor. Единственное исключение этого набора,
PS2PartitionRenderable, вызывает отдельный ctor20DD70; он проверен своим окном.
Первый статический inspector остановился на этой разнице, второй корректно
разделяет inline factory и отдельный ctor. Старый source сохранён.

AnimationSerializer выделяет76 байт, остальные28 —20. Размеры не копируются
с PC и не считаются современным C++ ABI. Getter каждого primary table отдельно
сопоставлен с собственным registration record. Девять различных родительских
constructors проверены через их vtable/getter: Serializer,NodeSerializer,
RenderableSerializer,RenderNodeSerializer,ModelSerializer,ProjectionSerializer,
PartitionNodeSerializer,PartitionRenderableSerializer,NavigationSetSerializer.
В этом наборе их identity совпадает с registered parent; общее правило
«RTTI всегда равен физическому наследованию» из этого не выводится.

## Вторичный интерфейс и предел исполнения

[PS2 probe](../../research/probe_ps2_serializer_dispatch.py) проверяет87 маршрутов
первых трёх secondary slots29 классов. Три адреса общие у PartitionRenderable
и PS2PartitionRenderable; повторное исполнение не нужно.84 unique original
thunks завершились за1,474 с: J к конкретному callback, в delay slot
`addiu A0,A0,-16`. Secondary receiver превращается в complete-object pointer;
остальные аргументы и все bytes receiver неизменны. Остановка — перед первой
инструкцией настоящего callback; reader/writer/index body не подставлен и
не объявляется исполненным.

Для PC записаны первые три secondary entries, для PS2 — три adjustors и ещё
два наблюдаемых слова. Последние обычно1815B0/1814D0; у Cinematic четвёртое
наблюдаемое слово19A6A0. Отдельное поведение этого override ещё не проверено.
Число записанных слов не выдаётся за полную длину неизвестного интерфейса.

## Оценка и дальнейшее восстановление

PC CollisionInfoSerializer65 по просмотренному прежнему payload evidence;
остальные19 новых PC классов20. Все29 новых PS2 строк15: проверенные construction
и routing не означают восстановленный payload codec. Всего49 assessments,
из них одна reviewed-existing. Ни один класс не объявлен закрытым.

В восьми частичных C++ источниках пространственных serializers пока остались
прежние явно отмеченные null Clone ограничения: BSPNode,OctreeNode,
PartitionNode,PartitionRenderable,PartitionSystem,Zone,ZonePortal,
ZonePortalNode. Новые original PC
прогоны подтверждают concrete Clone и снимают эту исследовательскую неизвестность;
перенос подтверждённого метода в общий source — отдельный следующий шаг,
не изменение UI или самостоятельная копия алгоритма в tools.

Открыты непроверенные payload read/write/index, конкретные DX resource границы,
PS2 полные lifetimes и загрузка/запись, error/rollback/cyclic-reference случаи.
Старые PC payload исследования остальных13 классов сохраняют прежние оценки.
