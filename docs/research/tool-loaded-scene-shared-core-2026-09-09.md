# Загруженные Node, Skin и состав render support

Блок 10 цикла 9 сентября до 19:00. Перенесена обвязка runtime и подготовки
начальной позы; игровые алгоритмы не переписывались. Это контрольный этап,
не выпуск и не завершение всех ядер.

## Изменение

SparkplugSceneRuntime теперь сохраняет настоящий ResourceGraph и все его Node,
включая наследников. Удалено построение отдельного графа базовых Node из C# DTO.
Поза обновляется оригинальными виртуальными методами; Skin palette берётся
из фактических bone bindings, inverse-bind и world. DTO вызывающей стороны
выбирает нужные Skin, но не подменяет их кости/матрицы. Закрытие внешнего graph
handle не освобождает объекты, пока ими владеет scene. Host index Node→ordinal
устраняет линейный поиск при проверке каждой кости.

Неизменяемый снимок документа включает реальные Node parents/world,
начальные Skin palette и упорядоченные члены RenderNode, StaticRenderObject,
PartitionRenderable. Повторы ссылок сохраняются. Это каталог членства, не
visibility traversal и не порядок отрисовки кадра. PartitionSystem имеет
RTTI parent Node, но физический класс и world dispatch используют RenderNode.

RenderNode обновляет собственные render matrices. StaticRenderObject отдаёт
две независимые сохранённые матрицы. PartitionRenderable отдаёт исходную
общую identity: добавлены только getters подтверждённого состояния, без
изменения игровой семантики. Доказательства: original prepare 0x4D7260,
identity 0x760058 и CRT 0x6D38C0 в native-pc-partition-runtime.md.

Проверка Alfea02 поймала ошибку адаптера: физический поиск по предкам дошёл
до transform PartitionSystem/Zone и сдвинул уже размещённую геометрию комнаты.
Исправлен софт: скалярный compatibility placement принимает только фактический
support данного Model. Физический предок выбирает один существующий support,
но не создаёт связь. Иначе допустим единственный support; неоднозначность
явно отклоняется. Первоначальный провал сохранён вместе с успешной проверкой.

## Проверки

| Файл | Node | Supports | Members | Managed checks |
|---|---:|---:|---:|---:|
| Alfea01 | 471 | 1028 | 1141 | 1416 |
| Alfea02 | 489 | 861 | 1009 | 1470 |
| Icy | 88 | 1 | 12 | 328 |
| BloomX | 102 | 6 | 11 | 356 |
| PC menu | 835 | 143 | 207 | 2508 |

Итого 1985 Node, 2039 supports, 2380 членов, 6078 managed checks. В PC menu
110 разных Model и 97 повторных вхождений; снимок сохраняет все 207.
Alfea02 дополнительно содержит ParticleSystem среди членов support.
Python-потребитель C ABI сверил точные биты матриц/ID, 1296 palette matrices,
пять SAN кадров Icy xiwa с перемоткой назад, 32 негативных ABI проверки.
Внешний graph handle закрывался до последующих чтений scene/костей.

Свежих original microprobes не было: reused NodeWorld, Skin и spatial proofs.
Статические PC anchors отдельно подтвердили Zone/PortalNode world→0x421420
и PartitionSystem→0x48E710→0x4250F0. Native suites: AnimationRuntime 32,
NodeWorld 86, FullLoader 213, RenderNode 34, StaticRenderObject 52,
SpatialSerialization 74. Viewer/Importer/LVLcreator core consumer builds:
0 warnings/errors. Native сохраняет прежнее C4756 в spVertexBounds.h.

Общие SMO regressions прошли для Alfea01/02, PC menu, Icy, BloomX:
37288/36395/9355/1609/1904 assertions. Шестой файл igmenu_opt_ps2.smo
теперь явно не проходит runtime placement: его 53 spMaterialColorController
требуют пока неподтверждённого PC factory. Metadata inspection остаётся
отдельно доступным. Этот провал не объявлен успехом и не закрыт fake factory.

## Оставшиеся границы

SceneBuilder пока выбирает экземпляры прежним способом; полный перенос на
container/member occurrence — следующий блок. SkinBindingResolver остаётся
утилитой анализа inverse-bind для authoring/export, не источником runtime позы.
Name binding SAN — существующая политика инструмента, не доказательство
полного actor/skeleton binding игры. Scene init, visibility, gameplay tick,
camera-dependent branches и renderer execution этим блоком не реализованы.
Полная загрузка файлов с material-color factory остаётся приостановленной
на ранее зафиксированной исследовательской границе; пользователь уведомлён.

Evidence: research/tools-core-loaded-scene-block-2026-09-09.json.
