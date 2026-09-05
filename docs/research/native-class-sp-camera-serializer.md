# `spCameraSerializer`: camera projection block

Статус: RTTI/lifetime, target, storage-free ABI, наследование node contract и два
camera field ID подтверждены независимо в PC и PS2. Runtime `spCamera`, его
projection/viewport state и concrete leaves теперь реконструированы отдельным
проверяемым срезом.

Точный original source path не сохранился. Class ID — `0x440E53FB`, direct base —
`spNodeSerializer` (`0x4545848A`), target — `spCamera` (`0x18DF3845`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075EBE8` | `0x004AA690` |
| Initializer | `0x006D2F70` | `0x00483610` |
| Factory | `0x0043D230` (protected entry) | `0x001919F0` |
| Allocation | observed `0x14` | exact `0x14` |
| Destructor | `0x0043D200` | deleting `0x00191860` |
| Blank clone | `0x0043D2A0` | `0x00191910` |
| Registration getter | `0x0043D1F0` | `0x00191310` |
| Target hook | `0x0043D220` | `0x00191850` |
| Read | `0x0043D310` | `0x00191320` |
| Write | `0x0043D610` (protected entry) | `0x001915C0` |
| Primary vtable | `0x006E0780` | header `0x0048F7D0` |
| Secondary vtable | `0x006E0774` | header `0x0048F7F4` |

PS2 factory выделяет `0x14` и меняет только vptr базового node serializer. Camera
relationship-finalizer наследуется (`0x004639D0` PC, `0x00196A60` PS2), поэтому новых
resource relationships класс не вводит.

## Wire contract

После успешного `spNodeSerializer::Write` всегда записывается field `0` (`esfCamera`) с
четырьмя `float` в порядке:

1. near clip plane;
2. far clip plane;
3. view angle;
4. pixel aspect ratio.

Field `1` (`esfCamera2D`) — byte/bool и записывается только для true. Reader принимает
поля `0/1`, неизвестные пропускает, требует ненулевой target и после чтения очищает
camera dirty-флаг `0x100`. Перед field loop он инициализирует camera параметрами текущего
render viewport; соответствующие viewport/default/projection состояния теперь
сопоставлены в [`spCamera`](native-class-sp-camera.md), но полный manager-driven
load side effect ещё не переносился.

Portable serializer содержит доказанный field planner, четырёхэлементный payload shape,
target и clone. Он не выдаётся за stream codec; runtime camera живёт отдельно и
не подменяет manager/relationship framing.

## Открытые вопросы

1. Original header/source path и исходные enum/signature names.
2. Полный manager-driven viewport initialization и post-read dirty-mask contract.
3. Точное значение datablock encoding argument `5`.
4. Error/status/rollback paths и PC protected writer/factory.
5. Mutation round-trip и поведение field `1` в игре.
