# Восстановление Clone восьми пространственных сериализаторов

10 сентября 2026. Продолжение [независимой проверки сериализаторов](native-serializer-expansion-2026-09-10.md).
[Проверки и снимки исходников](../../research/spatial-serializer-clone-fix-2026-09-10.json).

## Оригинальное поведение и доказанная ошибка

У spBSPNodeSerializer, spOctreeNodeSerializer, spPartitionNodeSerializer,
spPartitionRenderableSerializer, spPartitionSystemSerializer, spZoneSerializer,
spZonePortalSerializer и spZonePortalNodeSerializer прежняя восстановленная
реализация vfunc_10 явно возвращала nullptr: Clone находился вне ранее изученного
среза чтения. Теперь восемь оригинальных PC lifetimes из предыдущего блока
подтверждают отдельный 20-байтовый объект нужного класса, регистрацию пары
source/destination, успешный Copy и удаление обоих объектов.

Дополнительно сохранены независимые окна методов PC и PS2 в
`local-data/results/native-cycle-20260910-1900/spatial-clone-fix/clone-windows/capture.json`.
PC вызывает конкретную factory, регистрирует пару через 00412F70, затем вызывает
виртуальный Copy **у source**, передавая destination. При false удаляет новую
копию и возвращает null. PS2 отдельно подтверждает выделение 0x14 байт,
регистрацию через 001050C0 и виртуальный Copy; это статическая проверка окон,
не новый полный PS2 lifetime. Базовый spSerializer Clone остаётся null.

| Класс (без префикса sp) | PC Clone | PS2 Clone |
|---|---:|---:|
| BSPNodeSerializer | 0044CE40 | 0019FF20 |
| OctreeNodeSerializer | 0044C870 | 001A0C60 |
| PartitionNodeSerializer | 0044B420 | 001A1EA0 |
| PartitionRenderableSerializer | 0044EC20 | 001A2590 |
| PartitionSystemSerializer | 0044AE80 | 001A2B30 |
| ZoneSerializer | 0044D6B0 | 001A3FF0 |
| ZonePortalSerializer | 0044DD30 | 001A39A0 |
| ZonePortalNodeSerializer | 0044E580 | 001A3180 |

## Изменение общего кода и проверка

Восемь методов используют собственную существующую Create factory и общий
`spSerializerClone.h`: регистрация в существующем spCloneManager, виртуальный
Copy, возврат либо уничтожение новой копии. Этот header — наша организация
повторяющегося восстановленного алгоритма, не новый класс оригинального движка.
Дополнительных реализаций для tools или C# нет.

В существующий spSpatialSerializationTests добавлены 48 проверок восьми классов:
ненулевая независимая копия с конкретной RTTI, регистрация source/destination,
новый объект при следующем Clone, очистка временной карты после корневой операции
и сохранение null-контракта базы. До исправления тест завершился ошибкой
`native concrete spatial serializer Clone allocates`; после — **145/145**,
включая 97 прежних проверок чтения и владения.

Собрана только цель ViewerSpatialSerializationChecks, не Viewer/UI и не релиз;
инкрементальная сборка с двумя workers. Логи before/after сохранены рядом с
native windows. Уже проверенные оригинальные lifetimes повторно не запускались.
Ошибку старого stub устранили по игре; ожидаемые приложением данные не менялись.
Полнота payload writer/index и всего пространственного runtime не заявляется.
Дополнительных процентов за перенос уже учтённого знания нет.
