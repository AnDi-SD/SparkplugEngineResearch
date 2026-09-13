# Восстановление Clone восьми пространственных сериализаторов

## Оригинальное поведение и доказанная ошибка

У spBSPNodeSerializer, spOctreeNodeSerializer, spPartitionNodeSerializer,
spPartitionRenderableSerializer, spPartitionSystemSerializer, spZoneSerializer,
spZonePortalSerializer и spZonePortalNodeSerializer прежняя восстановленная
реализация vfunc_10 явно возвращала nullptr: Clone находился вне ранее изученного
среза чтения. Теперь восемь оригинальных PC lifetimes из предыдущего блока
подтверждают отдельный 20-байтовый объект нужного класса, регистрацию пары
source/destination, успешный Copy и удаление обоих объектов.

| Класс (без префикса sp) | PC Clone | PS2 Clone |
| --- | ---: | ---: |
| BSPNodeSerializer | 0044CE40 | 0019FF20 |
| OctreeNodeSerializer | 0044C870 | 001A0C60 |
| PartitionNodeSerializer | 0044B420 | 001A1EA0 |
| PartitionRenderableSerializer | 0044EC20 | 001A2590 |
| PartitionSystemSerializer | 0044AE80 | 001A2B30 |
| ZoneSerializer | 0044D6B0 | 001A3FF0 |
| ZonePortalSerializer | 0044DD30 | 001A39A0 |
| ZonePortalNodeSerializer | 0044E580 | 001A3180 |

Восемь методов используют собственную существующую Create factory и общий
`spSerializerClone.h`: регистрация в существующем spCloneManager, виртуальный
Copy, возврат либо уничтожение новой копии. Этот header — наша организация
повторяющегося восстановленного алгоритма, не новый класс оригинального движка.
Дополнительных реализаций для tools или C# нет.
