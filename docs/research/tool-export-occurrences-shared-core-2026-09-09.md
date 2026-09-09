# Exporter и SMO importer: фактические размещения общей сцены

Блок 13 цикла 9 сентября до 19:00. Exporter больше не собирает сцену вторым
обходом физических MeshData и StaticRenderObject. Он использует SmoViewer.Scene,
который получает actual loaded supports/Model/Skin через общий native graph.
Игровые классы и SparkplugViewerNative.dll в этом блоке не менялись.

## Изменение потребителей

Разделены три сущности: физическая геометрия MeshData, вариант её использования
конкретным Model/Skin и размещение по container/member slot. Повторный Model
не теряется, разные материалы одного Mesh сохраняются. Идентификаторы файла
не заменяются выдуманными: транспортные индексы FBX хранятся отдельно.
Сконвертированные массивы геометрии общие между вариантами одного Mesh.

GLB переиспользует geometry accessors, FBX — rigid FbxMesh attributes после
точного сравнения буферов. Skin deformer создаётся в контексте размещения.
Private export protocol повышен до 4; bridge также читает прежний v3.
OBJ разворачивает все размещения и различает материалы вариантов.
В baked GLB отдельные буферы адресуются порядком placements, поэтому одинаковый
Model index не приводит к потере экземпляра или конфликту словаря.

Выборочный экспорт требует конкретного slot при повторном Model. Splitter
требует Mesh/Model key при нескольких вариантах. CLI/GUI только подключены к
этому ключу; имена отдельных файлов включают исходный Model index. Layout UI
не менялся. Static selection явно отклоняет Skin вместо потери skinning.

SMO importer preview/geometry adapter теперь разворачивает placements общей
сцены и берёт собственный world каждого слота. Native SMO-to-SMO transfer этим
DTO не пользуется. MATERIAL_IMPORT_SHAPE явно отклоняет несколько passes/layers:
ImportedScene ещё не может хранить их без потерь. Пользователь уведомлён;
расширение промежуточного формата остаётся отдельной задачей.

## Проверки и расход памяти

| Файл | Физических Mesh | Вариантов | Размещений | Проверок GLB/FBX |
|---|---:|---:|---:|---:|
| Alfea01 | 554 | 1141 | 1141 | 35898 |
| Alfea02 | 702 | 1008 | 1008 | 32352 |
| Icy | 12 | 12 | 12 | 402 |
| BloomX | 11 | 11 | 11 | 369 |
| PC menu | 99 | 110 | 207 | 6513 |

Всего 2379 placements, 2282 variants, 75534 assertions. GLB прочитан тестовым
JSON reader, FBX — независимым SDK importer через новую inspect-export команду.
Проверены исходные reference identities, material RGB проекции, geometry counts,
world matrices и ровно один Skin deformer у skinned placement. Максимальная
FBX world ошибка 0,0009834364 игровых единиц; pose допускает float/TRS roundtrip.
Это не проверка всех material shaders или игрового кадра.

Выбранный маленький фрагмент PC menu: 60 checks выборки, split, baked GLB, OBJ
и отклонения конфликтующей FBX geometry. SMO importer: menu 207 placements /
1605 checks, Icy 12 / 3744, включая явный отказ для multipass BloomX.
Независимый v3 binary fixture прочитан SDK: ещё 4 checks совместимости.

У Alfea01 geometry buffers GLB занимают 5094572 вместо 9047780 байт при
копировании для каждого варианта; у Alfea02 — 5213960 вместо 6567460.
Это объём конкретных geometry buffers, не замер всего RSS или ускорения FPS.
Два build workers; native FBX и пять .NET consumer projects собраны успешно.
Первый FBX build не получил FileTracker доступ в sandbox; тот же локальный
build прошёл после автоматического разрешения исполнения вне sandbox.
Компиляторные ошибки новых тестов исправлены, журналы попыток сохранены.

## Оставшиеся границы

LevelOnly сохраняет прежний host-фильтр по storage owner поверх actual slots;
он не выдаётся за игровой culling. Неподдержанный ParticleSystem Alfea02 явно
остаётся в диагностике общей сцены. Полный материал хранится в LoadedMaterial,
но старые правила target diffuse/alpha и однопроходная texture projection ещё
не заменены полным material runtime. Многопроходное визуальное соответствие,
UV/texture clock и SAN-to-target world animation остаются дальнейшей работой.

Исходные доказательства поддержки slots переиспользованы из блоков 10–11;
original EXE и native engine suites без изменения их кода не перезапускались.
UI визуально не проверялся, релиз не упаковывался.

Evidence: research/tools-core-export-occurrences-block-2026-09-09.json.
