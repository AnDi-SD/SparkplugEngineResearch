# Нативный класс `spNode`

Статус: class identity, direct base, concrete factory, exact PC/PS2 allocation,
local transform, parent/child lifetime, collision-container boundary, flags,
recursive enabled-state и deep hierarchy clone подтверждены двумя executable.
World-transform update, scene registration и `spCollisionInfo` пока остаются
evidence-only и не подменяются придуманным renderer API.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x695C0F65 / spNamedObject` | same |
| Registration / initializer | `0x0075DD88 / 0x006D27E0` | `0x004AB1D0 / 0x00483E10` |
| Factory / allocation | `0x00421E20 / 0xB4` | `0x001A9160 / 0xC0` |
| Constructor | protected entry `0x00421BA0` | `0x001A8EE0` |
| Destructor | deleting `0x00422220` | body `0x001A8CF0` |
| Clone / copy | `0x00421E80 / 0x00421F80` | `0x001A9090 / 0x001A5BE0` |
| Registration getter | `0x00421D50` | `0x001A5AB0` |
| Primary vtable | `0x006DC4F4` | header `0x004902F0` |

Путь implementation/header `spNode` не найден. В executable присутствует лишь
точный serializer path `Z:\Sparkplug\Code\Sparkplug\spNodeSerializer.cpp`;
поэтому reconstructed `Code/Sparkplug/spNode.*` лежит в доказанном модуле, но
помечен как inferred declaration/implementation path.

## Layout и platform ABI

Общий смысловой prefix совпадает:

| Offset | Size | Роль |
|---:|---:|---|
| `+0x00` | `0x14` | `spNamedObject` |
| `+0x14` | `0x0C` | список дочерних `spNode` |
| `+0x20` | `0x0C` | local position |
| `+0x2C` | `4` | non-owning parent `spNode*` |
| `+0x30` | `0x0C` | local scale |
| `+0x3C` | `4` | scene/runtime registration link; точный тип неизвестен |
| `+0x40` | `0x24` | local 3×3 orientation matrix |
| `+0x64` | platform | контейнер intrusive `spCollisionInfo*` |

На PC collision vector занимает `0x10` (`allocator/begin/end/capacityEnd`),
после него лежит opaque cached-world блок `+0x74..+0xAF`, а flags находятся по
`+0xB0`; итоговый размер `0xB4` прямо задан factory allocation. На PS2
контейнер занимает `0x0C`, вслед за ним явно видны cached world position
`+0x70`, scale `+0x80`, orientation `+0x90`, flags `+0xB4` и восемь байт
alignment padding; factory выделяет `0xC0` с выравниванием 16.

Это реальная ABI-разница: cached fields не следует механически сдвигать между
платформами или представлять одним host-layout. Exact структуры находятся в
`Analysis/PC/SparkplugAbi.h` и `Analysis/PS2/SparkplugAbi.h`.

## Constructor и local transform

Чистый PS2 constructor сначала создаёт `spNamedObject` и child container, затем
задаёт position `(0,0,0)`, scale `(1,1,1)`, identity local orientation,
пустой collision container и identity cached world state. Последним он пишет
flags `0x00070A00`. PC constructor защищён, но serializer/copy/runtime accesses
независимо подтверждают те же смысловые offsets до PC flags `+0xB0`.

Native runtime хранит rotation как матрицу `3×3`, хотя SMO serializer переводит
её в quaternion XYZW. Portable `spNode` поэтому хранит matrix, не выдавая
serializer-представление за layout класса.

## Флаги и найденные методы

Serializer обеих платформ подтверждает маски:

| Mask | Сериализованное свойство |
|---:|---|
| `0x00000400` | `esfNodeIsStatic` |
| `0x00000800` | `esfNodeIsAnimated` |
| `0x00001000` | `esfNodeIsBone` |
| `0x00100000` | billboard axis value `1` |
| `0x00200000` | billboard axis value `2` |

PS2 `0x001A5B00` и соответствующий PC vtable slot переключают mask `0x200` и,
если третий аргумент ненулевой, рекурсивно вызывают тот же slot у всех детей.
Original имя флага не найдено; `EnabledMask` — аналитический alias. Другой
метод `0x001A5AD0` поднимается по parent `+0x2C` и возвращает root.

Старое corpus-описание считало отсутствующий legacy field 8 равным `false`.
Это не является constructor-default текущего runtime: constructor PS2 явно
включает `0x800`. Текущие writer-функции всегда сохраняют field 8; для 369
старых PC-объектов без поля точное сочетание reader semantics и исторической
версии формата пока не доказано. Инспектор может продолжать использовать
форматный fallback `false`, но он теперь явно отделён от runtime constructor.

## Иерархия, destruction и clone

Parent хранится non-owning, children — в intrusive-reference list. Attach/detach
пути меняют parent и список, защищаются от повторного parent и уведомляют
глобальный scene manager; полная семантика последних уведомлений ещё не названа.
Destructor последовательно:

1. удаляет/release-ит все collision entries;
2. отсоединяется от parent;
3. отсоединяет всех детей;
4. снимает `+0x3C` с глобальной регистрации;
5. уничтожает оба контейнера и `spNamedObject`.

Copy сначала очищает отношения destination, вызывает `spNamedObject` copy,
переносит flags, local orientation, position и scale, оставляет parent null,
затем глубоко клонирует collisions и детей через общий clone manager. Cached
world state не копируется. Portable слой повторяет доказанную часть для child
tree; collisions остаются закрыты до реконструкции `spCollisionInfo`.

Host-ownership детей представлен `shared_ptr`, что безопасно имитирует
intrusive references, но не претендует на ABI. Attach отвергает null, self,
повторного parent и цикл. Это дополнительная host-проверка, а не утверждение о
наличии всех этих guard-веток в оригинале.

## Связи и оставшееся неизвестным

`spTemplateInstance` теперь создаёт настоящий portable `spNode` с именем
`Instance Root`; прежний dependency-safe `spNamedObject` seam удалён. Следующие
registration-зависимости `spLight`/`spLightData`, `spRenderNode` и scene/partition
классы могут наследовать восстановленную основу без повторного изобретения
transform-графа.

Открыты: original header/TU и method names, тип `+0x3C`, точные dirty/cache masks,
весь world-transform update, bounds aggregation, recursive name lookup,
scene-manager callbacks, collision ownership API и serializer reader behavior
для legacy отсутствующего `esfNodeIsAnimated`.
