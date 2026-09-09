# Ядра tools — цикл 9 сентября до 19:00 МСК

08:12 МСК: пользователь поручил продолжать перенос общей логики, следующий
отчёт в19:00 МСК. Редакторские исключения отложены до LVLcreator; альтернативные
алгоритмы не согласованы. PC остаётся эталоном, PS2 используется по конкретной
пользе; UI вторичен. Исходный checkpoint1495999, Viewer4a407fd, Texture1ddb50f.

Проверены правила и предыдущий результат. Первый следующий шаг — сохранённый
StaticRenderObject scout и перенос нужного reader без C#-дублирования.
Уточнено: packed combiner120 — cached member, standalone/physical bytes84;
читать120 из файла без исследования реального потребителя не следует.

Текущий отчёт: [цикл](../../docs/research/tools-core-cycle-2026-09-09-1900.md).

Блок1: восстановлены StaticRenderObject и его reader/writer/indexer.44FE30 —
индексация references, не header factory. Пять fresh original scalar sections
совпали побайтно с source/CABI; общий RenderNode support выделен без смены
семантики. Static52, RenderNode34, FullLoader213, host graph registration прошли.
Старые ограничения C# матриц удалены; legacy inverse authoring перемещён в
Editing без изменения и оставлен на LVLcreator review. Viewer/Importer/LVLcreator
CoreTests builds прошли0 warnings/errors. Детали/выборка/ограничения:
`docs/research/tool-static-render-object-shared-core-2026-09-09.md`.

Блок2: общий MaterialData reader подключён к C# вместо самостоятельного
декодера; инспекция ID не создаёт substitute referents. Original9 selected
cases и3 host guards прошли,6 полных source reader/writer captures совпали.
MaterialSerialization554/Controller393/Color225/FullLoader213 прошли;
5 SMO:37097/36322/9326/3001/1573 assertions. Сборки Viewer/Importer/LVLcreator
CoreTests без предупреждений/ошибок. Проверка текущего DTO одним слоем на проход
остаётся явным host-пределом, native поддерживает8×8. Следующая задача Model/Skin.
Досье: `docs/research/tool-material-reader-shared-core-2026-09-09.md`.

Блок3: Model и Skin C# readers заменены общими исходными секциями; metadata
палитра не создаёт substitute Node. Original11/source writer/ABI cases совпали,
включая alpha256→true, repeat/clear palette и raw NaN/Inf matrix bits.4 guards;
C++ Skin119/Material554/FullLoader213. Старые Material9 original captures
повторно использованы для проверки нового общего reference helper без повторной
эмуляции.5 SMO:37101/36326/9330/3005/1577 assertions;3 consumer builds чистые.
Досье: `docs/research/tool-model-skin-reader-shared-core-2026-09-09.md`.

Блок4: общий reader AnimTexController и исходный выбор ключа TextureTrack.
Пять original/source captures,33 runtime selections и повторное использование
inline original graph прошли. C++ Controller488/Material554/FullLoader213;
5 SMO:951/1933/1753/1871/36330 assertions;3 consumer builds чистые.
Старые BloomX tests исправлены по реальным связям и исходным opaque BGRA bytes.
Это перенос чтения и индексатора; looping clock/UI bindings ещё не мигрированы.
Досье: `docs/research/tool-animated-texture-reader-shared-core-2026-09-09.md`.

Блок5: UV/MaterialColor field readers используют общие Trans/Color/Function
serializers.7 свежих original scalar-state captures и4 guards совпали;
UV386/Color118/MaterialColor225/FullLoader213 прошли.5 выборочных SMO:
750/748/802/937/36350 assertions;3 consumer builds0 warnings/errors.
Исправлена C# подстановка scale defaults удалением дублирующего decoder.
Досье: `docs/research/tool-material-functions-shared-core-2026-09-09.md`.

Блок6: общий material snapshot для полного DTO, цвета и состояний. Два parser
удалены; последняя запись состояний и чёрный diffuse больше не теряются.
9 прежних original captures,3 empty-pass ABI cases, Material554/FullLoader213
прошли.5 SMO:37182/36353/9341/3069/1588;3 consumer builds чистые.
Медиана узкого повторного чтения10 материалов:5,8782→1,4136ms;
managed allocations-17,16%, без заявления общего ускорения Viewer.
Досье: `docs/research/tool-material-inspection-shared-core-2026-09-09.md`.

Блок7: общий каталог Model/Skin references, скалярные материал-потребители,
active mesh и shared-instance metadata.5 SMO:37284/36389/9343/1888/1608;
3 consumer builds проверены. Native DLL неизменна, original/source suites
не повторялись. Alfea02:83 общих mesh с разными material IDs у потребителей,
5 NULL; Icy9 и BloomX4 nonlocal material links. Texture binding и фактическое
применение материала отдельного экземпляра продолжаются следующим блоком.
Досье: `docs/research/tool-renderable-references-shared-core-2026-09-09.md`.

Блок8: устранена следующая зависимость material runtime: семь пространственных
serializers и нужные реальные ресурсы подключены в полный ResourceGraph.
Direct children/root/payload передаются единственному владельцу; borrowed
Zone roots и PortalNode portals сохраняют идентичность, collision links
снимаются взаимно. Все14 original/source reader states совпали;74 spatial
guards,298 reference,213 full loader,34 render-node,54 collision checks прошли.
Alfea02:4266 ресурсов/489 Node; database FAT metadata и Node hierarchy совпали.
Сработал host cap4096, не глубина; после замера памяти он поднят до8192 в
приложении. CPU graph load около62–65ms, peak47,2MiB в отдельном процессе.
5 коротких managed regressions и3 consumer builds прошли. Source headers
содержат прежнее C4756 в spVertexBounds; .NET builds имеют0 warnings/errors.
Octree и последующее подключение material/texture runtime остаются в работе.
Досье: `docs/research/tool-spatial-readers-shared-core-2026-09-09.md`.

Блок9: удалён старый1154-строчный texture resolver. Actual loaded ResourceGraph
отдаёт все material passes/layers, canonical IDs, UV и texture end-time keys;
BGRA upload берётся из выбранной native CPU texture. Lazy cache оставляет один
снимок на документ, native graph после копирования освобождается. Исправлены
материалы существующих shared instances и перенос чужой Skin palette в Model.
Пять файлов:30432 snapshot/scene assertions; Python/C#168 pixel hashes совпали,
31 ABI guard прошёл. Native Material554/Controller488/FullLoader213 прошли.
Шесть общих SMO regressions:37288/36395/9355/3093/1609/1904 assertions.
Viewer/Importer/LVLcreator CoreTests builds:0 warnings/errors.
BloomX имеет три passes и два38-key controllers; прежний равномерный playback
не подставляется. Пользователь уведомлён: многопроходный frontend/UV/точный
clock ещё не подключены, часть прежних имитаций эффектов временно отключена.
Полный draw list/placement и renderer state execution остаются следующими.
Досье: `docs/research/tool-loaded-materials-shared-core-2026-09-09.md`.

Блок 10: Node/Skin runtime сохраняет actual ResourceGraph, derived virtual
world dispatch и исходные Skin bindings. Начальная поза больше не берётся
из обращения inverse-bind. Добавлен ordered support snapshot с повторами.
Пять файлов: 6078 managed checks, 1985 Node, 2039 supports, 2380 members;
1296 palette matrices, пять SAN frames и 32 ABI guards совпали с C ABI.
Native suites 32/86/213/34/52/74; три consumer builds 0 warnings/errors.
Регрессия комнаты Alfea02 исправлена в адаптере выбора placement, не в игре.
Пять общих SMO tests прошли; PS2-tagged menu не проходит настоящий runtime
из-за 53 MaterialColorController с пока неподтверждённым PC factory.
Провал и исходные логи сохранены, fake graph не возвращён, пользователь уведомлён.
Досье: `docs/research/tool-loaded-scene-shared-core-2026-09-09.md`.

Блок 11: общий SceneBuilder теперь перечисляет actual support slots вместо
physical mesh + Static-only extension. Alfea01 1141, Alfea02 1008 размещений;
PC menu 207 ссылок/110 Model. Удалён name filter trail_mesh; неподдержанный
ParticleSystem явно диагностируется. Skin world getter разделяется с Render
body, повторный container transform не применяется. Viewer key включает slot.
18057 managed checks, 2379 C ABI comparisons, 21 guards; LVL Workspace 4726
checks. Native SkinRender 237, SkinSerialization/FullLoader passed; четыре
consumer builds 0 warnings/errors. Релиз/визуальный UI-прогон не выполнялись.
Пользователь уведомлён: повторные ссылки нельзя независимо адресовать старой
моделью команд LVLcreator. Workspace их сохраняет; прежнее молчаливое TryAdd
склеивание заменено явным отказом и отложено до работы над authoring.
Досье: `docs/research/tool-render-occurrences-shared-core-2026-09-09.md`.

Блок 12: Exporter больше не пересчитывает Node hierarchy собственным FK и
не подставляет inverse-bind pose. Actual ResourceGraph даёт все derived Node
world/parents; форматный adapter только преобразует координаты/local matrix.
Пять GLB прочитаны обратно: 1985 Node, 37720 assertions; max linear error
5.364418e-7, translation 0.00048828125. Три consumer builds 0 warnings/errors.
Native неизменен, probes/suites не повторялись. Полные occurrences и material
variants Exporter остаются следующей работой; статическая поза проверена отдельно.
Досье: `docs/research/tool-exporter-node-pose-shared-core-2026-09-09.md`.

Блок 13: Exporter переведён на общий SceneBuilder и все actual support slots.
Mesh/Model variants и repeated slots больше не склеиваются. Геометрия общая,
материал/Skin собственный; private FBX v4 имеет отдельные transport ordinals,
v3 читается. SDK readback пяти сцен: 2379 placements, 2282 variants, 75534 checks.
Selection/baked/OBJ/FBX guards 60, importer menu/Icy 5349, v3 fixture 4.
Importer больше не теряет repeated placements. Несколько passes/layers явно
отклоняются его ограниченным ImportedScene; static selection не отбрасывает Skin.
Пять .NET builds и native FBX passed. Full shader/material clock остаются далее.
Досье: `docs/research/tool-export-occurrences-shared-core-2026-09-09.md`.

Блок 14: Materials API на том же live ResourceGraph выполняет actual controller
accumulation / pass UV+AnimTex update / material color-frame entry. Один shared
projector используется runtime и immutable snapshot. Два старых uniform clocks
удалены из frontend. Пять файлов: 2191 material, 88 controllers, 86 UV updates,
13787 checks. Python 14 playback events/73 checks/37 guards, snapshot 30938.
Native MaterialController/UVFunction/FullLoader и пять builds прошли. Game code
не менялся; original proofs reuse. Автоматический frontend frame ещё не подключён,
MaterialColorController factory не подменялась.
Досье: `docs/research/tool-material-runtime-shared-core-2026-09-09.md`.

Блок 15: общий Octree reader и metadata bridge; 4 original/source совпадения,
15 ABI checks / 11 guards, тестовый полный граф 9 объектов. Четыре уровня
дали 245 Octree / 984 checks. Full loader этих уровней останавливается раньше
на SkyBox; добавлено указание class/object ID, SkyBox исследуется следующим.
SpatialSerialization/FullLoader и пять managed builds passed. В Native bridge
возвращены LF после случайного CRLF прошлого блока. Релиза нет.
Досье: `docs/research/tool-octree-reader-shared-core-2026-09-09.md`.

Блок 16: actual SkyBox/world/dedicated draw boundary и original регистрация
RenderNodeSerializer вместо нового serializer. Exact-class guard был ошибкой
реконструкции; три original/source reader-writer matches и два raw world matches.
race_01 + mini_level_date_02: 3399 объектов, 979 occurrences, 18 + 7350 checks.
Четыре native suites, пять consumer builds. Sky metadata projector общий,
special pass identity сохранён; в обычном GL pass небо пропущено с диагностикой.
Camera-follow frontend ещё открыт. Далее OcclusionVolume/NavigationGraph,
дополнительный battle_01 также требует LensFlare. Релиза нет.
Досье: `docs/research/tool-skybox-shared-core-2026-09-09.md`.

Блок 17: actual NavigationGraph/NavigationSet/MeshNavigationSet/Portal,
четыре serializers и общий immutable bridge. Удалены три C# reader с
дополнительными ограничениями маршрутов. 6 scalar / 3 relationship /
2 MeshBV raw captures совпали, scalar ABI также 6/6. Три уровня:
10 228 объектов, 13 sets, 10 portals, 20 623 cells / 21 533 checks.
FullLoader213/ReadReference298/Navigation34, пять builds прошли.
Repeated populated table resize в оригинале дал duplicate release, такой ввод
явно отклоняется; пользователь уведомлён. Маленький test_world_navmesh требует
source-only TextureData reader; уточнена inline diagnostic chain. Следующая
независимая задача — этот texture input. Релиза и полной готовности ядер нет.
Досье: `docs/research/tool-navigation-readers-shared-core-2026-09-09.md`.

Блок18: actual LensFlare/Quad ownership и LensFlareSerializer. Пять original-PC
reader states совпали, включая counted array, NULL primary preserve, raw floats
и повторный resize. Старый C# reader удалён, неправильный optional-glare layout
исправлен. PC LensFlare dtor capped88B422, не повторялся; PS2 static destructor
и отдельный успешный PC Quad lifecycle закрыли необходимое владение.
ABI21 (15guards), managed20, LensFlare15/FullLoader213; пять builds.
Battle01/Gardenia03 требуют OcclusionVolume, инспектор полного LensFlare там
пока недоступен. Source-less TextureData не подправлялся. Релиза нет.
Досье: `docs/research/tool-lens-flare-shared-core-2026-09-09.md`.

Блок19: пять actual spOcclusionVolume topology leaves,15 PC/source matches,
original constructor scalar defaults и Node-only clone. PS2 static помогает
разделить remaining Init; capped full Init не повторён. Полной загрузки нового
класса нет, fake serializer не добавлен. OcclusionTopology38/FullLoader213.
Diagnostic fixture потребовал действительное имя, существующие CRT helpers и
штатное отключение console output; geometry branches не заменялись. C# не менялся.
Досье: `docs/research/tool-occlusion-topology-shared-core-2026-09-09.md`.
