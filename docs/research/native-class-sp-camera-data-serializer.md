# `spCameraDataSerializer`: thin camera wrapper

Статус: RTTI/lifetime, storage-free ABI, wrappers и фактический target подтверждены в PC
и PS2. Имя класса не совпадает с target ожиданием: обе сборки возвращают `spCamera`, а
не `spCameraData`.

Class ID — `0x759F1687`, direct base — `spCameraSerializer` (`0x440E53FB`), target —
`0x18DF3845` (`spCamera`). `spCameraData` имеет отдельный Class ID `0x24BB4C41`, но этот
serializer его не возвращает.

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075EDD0` | `0x004AB170` |
| Initializer | `0x006D3060` | `0x00483DD0` |
| Factory | `0x00441040` (protected entry) | `0x001A5A20` |
| Allocation | observed `0x14` | exact `0x14` |
| Destructor | `0x00440FF0` | deleting `0x001A58D0` |
| Blank clone | `0x004410B0` | `0x001A5940` |
| Registration getter | `0x00440FE0` | `0x001A57D0` |
| Target hook | `0x00441010` | `0x001A5840` |
| Signature/load helper | `0x00441120` | `0x001A5850` |
| Read wrapper | `0x00441020` | `0x001A57E0` |
| Write wrapper | `0x00441180` | `0x001A57F0` |
| Primary vtable | `0x006E1FC8` | header `0x00490290` |
| Secondary vtable | `0x006E1FBC` | header `0x004902B4` |

Reader передаёт управление `spCameraSerializer::Read`; writer вызывает общий camera
writer и добавляет только собственную диагностическую границу. Relationship finalizer
также наследуется. PS2 factory выделяет ровно `0x14`, а constructor path меняет только
vptr, поэтому derived members не подтверждены.

Portable-класс повторяет фактическую RTTI-цепочку, target и clone и наследует camera
field planner. Отдельный payload или подмена target на `spCameraData` не добавлены.

## Открытые вопросы

1. Почему native registry связывает `spCameraDataSerializer` с `spCamera`.
2. Original header/source path и исходные wrapper/signature names.
3. Что делает load helper после проверки `SBOO`-подобной сигнатуры.
4. Direct PC allocation и protected factory.
5. In-game dispatch: где выбираются camera/camera-data serializer records.
