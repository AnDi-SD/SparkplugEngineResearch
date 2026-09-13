# spCameraDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spCamera](../../../Sparkplug/Code/Sparkplug/spCamera.h), [spCameraData](../../../Sparkplug/Code/Sparkplug/spCameraData.h), [spCameraDataSerializer](../../../Sparkplug/Code/Sparkplug/spCameraDataSerializer.h), [spCameraSerializer](../../../Sparkplug/Code/Sparkplug/spCameraSerializer.h).

Class ID — `0x759F1687`, direct base — `spCameraSerializer` (`0x440E53FB`), target —
`0x18DF3845` (`spCamera`). `spCameraData` имеет отдельный Class ID `0x24BB4C41`, но этот
serializer его не возвращает.

Reader передаёт управление `spCameraSerializer::Read`; writer вызывает общий camera
writer и добавляет только собственную диагностическую границу. Relationship finalizer
также наследуется. PS2 factory выделяет ровно `0x14`, а constructor path меняет только
vptr, поэтому derived members не подтверждены.

Portable-класс повторяет фактическую RTTI-цепочку, target и clone и наследует camera
field planner. Отдельный payload или подмена target на `spCameraData` не добавлены.
