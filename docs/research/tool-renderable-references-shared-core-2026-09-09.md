# Ссылки Model/Skin для потребителей ресурсов

Седьмой блок цикла до19:00 МСК9 сентября. Каталог и скалярные потребители
переведены на восстановленные ссылки; texture binding и применение материала
к каждой отрисовываемой копии остаются следующим этапом.

## Реализация

SmoRenderableCatalog один раз вызывает общие Model/Skin decoders на документ.
Каталог хранит исходные Material/Fog/Mesh связи и Skin palette metadata,
индексируя renderables и всех потребителей меша. Не читает поля самостоятельно,
не создаёт игровые ресурсы и не выбирает fallback. NULL остаётся NULL.
ConditionalWeakTable привязан к неизменяемому документу. Ошибки reader доступны
через Issues; SceneBuilder добавляет их к DecodeErrors и использует готовые Skin
DTO вместо повторной загрузки тех же полей.

Color/RenderState ResolveAll больше не сопоставляют материалы и меши по порядку
соседей/числу объектов/logical offsets. Совместимый view физического MeshData
использует содержащий Model/Skin только если его фактическая финальная ссылка
указывает именно на этот меш. Runtime occurrences в каталоге адресуются по
ObjectIndex Model/Skin отдельно; материал не является свойством MeshData.

ActiveVisualResourceResolver заменяет ручной скан всех fields0 исходной
финальной mesh assignment, включая Skin. SharedMeshInstanceResolver использует
те же связи и настоящий Material ID экземпляра, удаляя повторный ID lookup и
линейный поиск всех детей каждого Model. Выбор placement по storage ancestor
остаётся прежней явно обозначенной политикой инспектора.

## Диагностика и проверки

На pristine Alfea02 все1008 Model/Skin разобраны, ссылаются на702 meshes;
у83 meshes потребители имеют разные material IDs,17 material references
ведут вне физического блока своего Model,5 Material links равны NULL.
Icy:12 renderables/12 meshes,9 nonlocal material links, без NULL/ошибок.
BloomX:11/11,4 nonlocal material links, без NULL/ошибок. Разные ID не означают
обязательно разные значения материала. Хеши файлов/сборки диагностического
reader и source сохранены. Полного corpus scan не было.

Синтетический случай двух Model с одним MeshData и различными материалами
проверяет отсутствие подбора по равным именам. Третий Model меняет ссылку mesh
повторным полем: каталог сохраняет последнее назначение и NULL material.
Пять реальных SMO прошли: Alfea0137284, Alfea0236389, PC-menu9343,
BloomX1888, Icy1608 assertions. Viewer/Importer/LVLcreator CoreTests builds
проверены. Native DLL и восстановленная логика не менялись, исходные suites
и original probes повторно не запускались; доказательства reader — блоки2/3.

Оставшаяся работа: TextureBindingResolver всё ещё содержит выбор по именам/
containment; SceneBuilder копирует material data физического mesh в shared
occurrence. Следующий блок должен устранить это по отдельным resource links.
Пять NULL материалов не разрешены догадкой: требуется подтверждённое состояние
выбора материала renderer. UI, original placement policy и полный scene load
этим блоком не объявляются готовыми.

Evidence: `research/tools-core-renderable-references-block-2026-09-09.json`.
