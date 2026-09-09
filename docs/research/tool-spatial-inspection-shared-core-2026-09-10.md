# Пространственные инспекторы на actual ResourceGraph

Первый блок цикла до 07:00 МСК 10 сентября. Восемь C# decoders — PartitionNode,
BSPNode, OctreeNode, PartitionRenderable, PartitionSystem, Zone, ZonePortal,
ZonePortalNode — заменены проекциями уже существующих восстановленных классов.
Их собственные parsers, defaults, требования writer order, cardinality и
нормализации BSP plane удалены. Игровые классы и readers в этом блоке не менялись.

Нативный `ResourceGraph::SpatialJSON` копирует getters фактических объектов.
`spv_graph_spatial_json` передаёт ограниченный неизменяемый снимок C#; это host
DTO, не формат игры и не вторая сериализация SMO. Float передаются битами UInt32,
чтобы JSON не изменял signed zero, NaN payload и infinity. Снимок создаётся один
раз на граф, а `SmoLoadedResources` кэширует C# данные для неизменного документа.

`SmoLoadedReference` содержит канонический объект каталога или NULL. Он не
выдумывает исходную wire encoding, inline size или ownership по итоговому
указателю. Порядок и повторные references сохранены. У partition явно доступны
actual parent и размер child storage; список Children содержит ненулевые слоты
с их настоящими индексами. У Portal сохранён raw OpenByte, отдельно IsOpen.
Неинициализированная BSP plane представлена NULL, неизвестный Octree pivot
остаётся явным отказом соответствующего inspector. Node внутри трёх DTO
по-прежнему обозначает authored metadata из общего Node reader, не новый runtime.

Скалярные/полигональные значения Inspector используют снимок. Исходные bytes
остаются в HexPreview; wire-reference сведения читает существующий общий
`spSerializer` prefix reader. Вывод инспектора не подменяет фактическую запись
файла, runtime visibility, Scene attachment или render traversal.

## Проверка

Предыдущее original evidence:
[общие spatial readers](tool-spatial-readers-shared-core-2026-09-09.md).
Архив `original-readers.json` содержит 14 ограниченных PC captures на настоящих
factory objects. Новый `ViewerSpatialSerializationChecks --capture` на тех же
секциях сравнил все состояния повторно; 14 совпадений. Исходный EXE/probe и
входы сохранены, архив не переписан. Это повторное сравнение текущих исходников
с прежними original observations, не заявление о 14 новых запусках игры.

Для каждого случая создан полноценный небольшой FFPS: нужные реальные
dependency resources читаются до target, после чего оригинальная target-секция
подаётся без изменения bytes. В частности, остаются повторы roots/portals/support
members, позднее изменение color и NULL assignment. Проекция ABI совпала со
всеми опубликованными полями original capture. Model debug colors в этом ABI
не публикуются; они проверены сравнением полного native source capture.

Дополнительные три fixtures проверяют raw BSP plane и Octree pivot с NaN,
infinity и signed zero, а также неизвестный constructor pivot. Они отдельно
обозначены как проверки передачи данных на подтверждённом reader-контракте.

Выборка из существующей базы, без полного повторного прохода:

| Файл pristine PC | Ресурсов | Представленный вариант |
| --- | ---: | --- |
| Levels/Alfea/Alfea01.smo | 4411 | BSP, Zone, Portal, PortalNode, support payload |
| Levels/Gardenia/Gardenia02.smo | 3217 | Octree, partition leaves, повторные ресурсы |

Вместе файлы содержат все восемь затронутых классов. Проверены полный состав
пространственных snapshots, канонические IDs и обратные parent links.
ABI-набор: 5578 checks; загрузка и извлечение снимка этих двух файлов заняли
примерно 0,26 и 0,16 секунды соответственно. Это не FPS или время GUI startup.
Native suites SpatialSerialization97, LightSerialization205 и FullLoader213
прошли. Managed-проекция проверила 560 пространственных объектов (включая
fixtures): 15 076 checks. Пять сборок Viewer/FormatTests/Exporter/Importer/
LVLcreator прошли без предупреждений и ошибок, в том числе после уточнения
direct-document Light transport. Общий FormatTests на одном Bloom projectile
прошёл647 assertions.

Локальные артефакты (вне Git):
`local-data/results/tools-core-cycle-20260910-0700/spatial-inspection/`,
`abi-2.json`, оригинальные/реальные fixture snapshots, native build log.
Первая попытка fixture payload использовала ошибочный ClassID модели в стенде;
loader явно отказал. Исправлен только fixture ClassID на оригинальный 763277DB.
Это не ошибка reader и не основание править игру/восстановленные классы.

## Явное ограничение исследовательских команд

Восемь старых Corpus-команд требовали совмещённого PC/PS2 corpus profile и
перезаписывали historical evidence. Они теперь требуют PC runtime, который
не доказывает PS2 runtime equivalence. Guard `SPATIAL_HISTORICAL_PROFILE`
останавливает несовместимый переанализ до записи БД; архивная база читается.
Wire provenance читается отдельно общим prefix reader, а не восстанавливается
из loaded reference. Пользователь уведомлён в ходе блока.

Рекомендуемое продолжение — раздельный PC runtime acceptance и отдельная
подтверждённая PS2 inspection/profiling операция. Старые числа полного корпуса
не превращены в результат текущей загрузки. Неподдержанные соседние ресурсы
также могут остановить whole graph: typed inspector сообщает отказ, raw fields
остаются доступны. Occlusion Init, Scene/visibility и spatial writer operations
этим блоком не закрыты.
