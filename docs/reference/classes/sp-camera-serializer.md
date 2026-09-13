# spCameraSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spCamera](../../../Sparkplug/Code/Sparkplug/spCamera.h), [spCameraSerializer](../../../Sparkplug/Code/Sparkplug/spCameraSerializer.h).

Статус: RTTI/lifetime, target, storage-free ABI, наследование node contract и два
camera field ID подтверждены независимо в PC и PS2. Runtime `spCamera`, его
projection/viewport state и concrete leaves теперь реконструированы отдельным
проверяемым срезом.

Точный original source path не сохранился. Class ID — `0x440E53FB`, direct base —
`spNodeSerializer` (`0x4545848A`), target — `spCamera` (`0x18DF3845`).

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

Portable serializer содержит доказанный field planner, четырёхэлементный payload shape,
target и clone. Он не выдаётся за stream codec; runtime camera живёт отдельно и
не подменяет manager/relationship framing.

## Открытые вопросы

1. Original header/source path и исходные enum/signature names.
2. Полный manager-driven viewport initialization и post-read dirty-mask contract.
3. Точное значение datablock encoding argument `5`.
4. Error/status/rollback paths и PC protected writer/factory.
5. Mutation round-trip и поведение field `1` в игре.
