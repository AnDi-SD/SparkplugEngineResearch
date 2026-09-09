# Общий читатель Octree для инструментов

Блок 15 цикла до 19:00. В `Sparkplug/` добавлен actual
`spOctreeNodeSerializer` (05BC2A91, parent 452AB84A), зарегистрирована пара
с существующим `spOctreeNode` (21A70829). C# inspector получает собственные
три вектора через тонкий C ABI; второй C# разбор этих полей удалён.

## Основание по игре

PC reader 44C8E0 сначала вызывает PartitionNode reader 44B660, затем читает
собственную секцию: field 0 → pivot +84, 1 → mins +B0, 2 → maxs +BC.
Порядок произвольный, повтор заменяет предыдущее значение, unknown пропускается,
omitted сохраняет состояние. Constructor обнуляет bounds, но оставляет pivot
неинициализированным. Нет вычисления midpoint, finite/order validation и
требования непустых восьми child slots. Это не исправление игрового алгоритма:
восстановлен отсутствовавший reader, сняты ограничения прикладного разбора.

Четыре bounded исполнения настоящих PC factories/readers/destructors проверили
empty, reordered/repeated values, raw NaN/Infinity/-0 и partial update с исходным
состоянием. Все выделения освобождены; максимум 8648 инструкций / 58768 bytes
arena, обычные ограничения 64 KiB / 100k instructions сохранены.
Source captures совпали во всех четырёх случаях. PS2 grammar, ранее найденная
по 001A0800, согласуется; нового исполнения PS2 в этом блоке не было.

## Подключение и проверки

Native graph использует настоящий reader и существующее Partition ownership.
`spv_graph_octree` возвращает реальные parent/child IDs и scalar state;
`spv_octree_fields_read` вызывает тот же reader для metadata inspector.
Признак pivotKnown отделяет неинициализированное игровое поле от нуля.
Managed DTO с обязательным Pivot явно отклоняет его отсутствие. Inherited
Partition metadata adapter пока сохраняет свои ограничения; это не full loader.

Независимый маленький тестовый FFPS с Octree и восемью PartitionNode children
загрузил полный граф из 9 объектов. C ABI: 15 assertions и 11 guards, включая
malformed bounds, null pointers, неверный class/ID. Это отдельная тестовая
сцена, не реальный уровень с удалёнными неподдержанными классами.

| PC файл | Octree nodes | Managed checks |
|---|---:|---:|
| Swamp/BMS_02 | 42 | 169 |
| Gardenia/Gardenia02 | 44 | 177 |
| Challenges/race_01 | 47 | 189 |
| RedF/RedF01 | 112 | 449 |

Итого 245 nodes / 984 checks на raw vectors и восьми наблюдаемых child slots.
Все четыре полных уровня пока останавливаются в FAT на отсутствующем
`spSkyBox` 7A7124AF. Диагностика FAT теперь указывает class ID и object ID;
правила загрузки не менялись. SkyBox становится следующей точечной задачей.

SpatialSerialization и FullLoader прошли; Viewer tests/WPF, Exporter, Importer
и LVL core tests собраны, 0 warnings/errors в пяти managed builds. Native
build attempt 2 имел исправленную const compilation error, attempt 3 успешен;
все логи сохранены. В Native bridge восстановлены прежние LF line endings
после случайного CRLF форматирования прошлого блока; это не изменение логики.

Native DLL SHA256:
`AF1CA1F095AD7228E43C18FE90E1E929F88D69AF5261B40A64C5BE0F6CE97ACE`.
Scene initialization, visibility scheduler и Octree writer этим не заявляются.
UI визуально не проверялся, релиза нет.

Evidence: `research/tools-core-octree-reader-block-2026-09-09.json`.
