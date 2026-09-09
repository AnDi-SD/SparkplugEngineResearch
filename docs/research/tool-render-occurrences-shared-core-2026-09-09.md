# Реальные экземпляры в общем SceneBuilder

Блок 11 цикла 9 сентября до 19:00. Перенесены подготовка сцены и подключение
ядра LVLcreator; frontend получил только минимальное изменение ключа геометрии.
Проверочные сборки не являются релизом.

## Что изменилось

SceneBuilder больше не строит список из физических MeshData плюс выборочных
StaticRenderObject instances. Он читает каждый действительный reference slot
загруженного RenderNode/StaticRenderObject/PartitionRenderable. Host identity
SmoRenderOccurrenceKey содержит container object index и member slot; это не
выдуманный сериализуемый класс. SceneObjectIndex обозначает потребляющий
Model/Skin. Повторные ссылки не удаляются, общий Mesh декодируется один раз.
Каждый потребитель сохраняет собственные material, Skin и rigid support node.
Фильтрация геометрии по имени trail_mesh удалена.

Новый C ABI spv_graph_render_occurrence возвращает входную матрицу конкретного
слота. Для Model это фактический support, для Skin — identity из общего
GetRenderWorldMatrixForAnalysis, используемого также восстановленным Render
body. Из тела вынесена та же константа с теми же битами; игровая семантика не
изменена. Основание: PC 0x46A240 и ранее подтверждённый Skin render chain.
Это предотвращает двойное применение размещения после world-space palette.

Viewer geometry key включает слот. Выбор/кадрирование объекта охватывают все
его вхождения одним проходом по геометрии. Дерево объектов не переделывалось.
LVLcreator Workspace сохраняет OccurrenceKey, имена и ID реального Model,
не заимствуя имя физического владельца общего mesh.

## Проверки и результат

| Файл | Старый Scene count | Новый count | Managed checks |
|---|---:|---:|---:|
| Alfea01 | 1050 | 1141 | 8546 |
| Alfea02 | 954 | 1008 | 7764 |
| Icy | 12 | 12 | 101 |
| BloomX | 11 | 11 | 93 |
| PC menu | 99 | 207 | 1553 |

Итого 2379 поддержанных occurrences и 18057 managed checks на пяти выбранных
файлах. В Alfea02 дополнительный ParticleSystem сохраняется в loaded membership
и выдаёт явную неподдержанную occurrence diagnostic, без подмены Model.
PC menu имеет 97 повторно включённых Model; все 207 слотов сохранены.
Проверены собственные material references, mesh identity, Skin palette ownership,
rigid support и побитовые input world; общий mesh разделяет одну DTO allocation.
Независимый Python C ABI consumer сверил все 2379 слотов и 21 негативный случай.

LVLcreator: Alfea01/Alfea02 сохранили все 1141/1008 placements до editable
документа; PC menu Workspace сохранил 207, после чего editable model явно
отклонил неоднозначное authoring. Всего 4726 workspace checks.
Native SkinRender 237/237 прошёл после выделения общего getter; также прошли
SkinSerialization и FullLoader. Original probes не повторялись: reused
доказательства Skin/render support и loaded graph. Четыре сборки (Viewer
FormatTests, Importer FormatTests, LVLcreator CoreTests, Viewer WPF app) прошли
с 0 warnings/errors. Визуальный UI-прогон не выполнялся.

## Границы и отложенный случай

Каталог членства не объявляется результатом native visibility или draw order.
ParticleSystem runtime, полный материал/renderer execution и прежние alpha
preview policies остаются отдельной работой. Exporter пока имеет собственный
старый scene adapter; его перенос на общий подготовленный состав — следующий
потребитель. Raw resource inspection отдельно от actual loaded scene сохранена.

В LVLcreator обнаружено молчаливое склеивание повторов через TryAdd ключа
(mesh, Model). Оно заменено явным REPEATED_RENDERABLE_AUTHORING: данные Workspace
сохранены, создание редактируемого документа остановлено. Пользователь уведомлён.
Рекомендация для будущей работы над LVLcreator: добавить адрес container/slot
в модель команд и отдельно определить семантику изменения общей Model reference.
Не присваивать искусственные file IDs и не дублировать ресурсы незаметно.

Evidence: research/tools-core-render-occurrences-block-2026-09-09.json.
