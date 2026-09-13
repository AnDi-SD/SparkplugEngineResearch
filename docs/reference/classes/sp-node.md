# spNode

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spCollisionInfo](../../../Sparkplug/Code/Sparkplug/spCollisionInfo.h), [spNode](../../../Sparkplug/Code/Sparkplug/spNode.h).

## Объект и поведение

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x695C0F65 / spNamedObject` | same |

### Layout и platform ABI

Общий смысловой prefix совпадает:

| Offset | Size | Роль |
| ---: | ---: | --- |
| `+0x00` | `0x14` | `spNamedObject` |
| `+0x14` | `0x0C` | список дочерних `spNode` |
| `+0x20` | `0x0C` | local position |
| `+0x2C` | `4` | non-owning parent `spNode*` |
| `+0x30` | `0x0C` | local scale |
| `+0x3C` | `4` | scene/runtime registration link; точный тип неизвестен |
| `+0x40` | `0x24` | local 3×3 orientation matrix |
| `+0x64` | platform | контейнер intrusive `spCollisionInfo*` |

На PC collision vector занимает `0x10` (`allocator/begin/end/capacityEnd`),
после него лежат cached-world position `+0x74`, scale `+0x80`, orientation
`+0x8C..+0xAF`, а flags находятся по
`+0xB0`; итоговый размер `0xB4` прямо задан factory allocation. На PS2
контейнер занимает `0x0C`, вслед за ним явно видны cached world position
`+0x70`, scale `+0x80`, orientation `+0x90`, flags `+0xB4` и восемь байт
alignment padding; factory выделяет `0xC0` с выравниванием 16.

Это реальная ABI-разница: cached fields не следует механически сдвигать между
платформами или представлять одним host-layout. Exact структуры находятся в
`Analysis/PC/SparkplugAbi.h` и `Analysis/PS2/SparkplugAbi.h`.

### Constructor и local transform

Чистый PS2 constructor сначала создаёт `spNamedObject` и child container, затем
задаёт position `(0,0,0)`, scale `(1,1,1)`, identity local orientation,
пустой collision container и identity cached world state. Последним он пишет
flags `0x00070A00`. PC constructor защищён, но serializer/copy/runtime accesses
независимо подтверждают те же смысловые offsets до PC flags `+0xB0`.

Native runtime хранит rotation как матрицу `3×3`, хотя SMO serializer переводит
её в quaternion XYZW. Portable `spNode` поэтому хранит matrix, не выдавая
serializer-представление за layout класса.

### Флаги и найденные методы

Serializer обеих платформ подтверждает маски:

| Mask | Сериализованное свойство |
| ---: | --- |
| `0x00000400` | `esfNodeIsStatic` |
| `0x00000800` | `esfNodeIsAnimated` |
| `0x00001000` | `esfNodeIsBone` |
| `0x00100000` | billboard axis value `1` |
| `0x00200000` | billboard axis value `2` |

PS2 `0x001A5B00` и соответствующий PC vtable slot переключают mask `0x200` и, если третий аргумент ненулевой, рекурсивно вызывают тот же slot у всех детей. Другой метод `0x001A5AD0` поднимается по parent `+0x2C` и возвращает root.

Это не является constructor-default текущего runtime: constructor PS2 явно включает `0x800`. Текущие writer-функции всегда сохраняют field 8; для 369 старых PC-объектов без поля точное сочетание reader semantics и исторической версии формата пока не доказано. Инспектор может продолжать использовать форматный fallback `false`, но он теперь явно отделён от runtime constructor.

### Иерархия, destruction и clone

Parent хранится non-owning, children — в intrusive-reference list. Attach/detach
пути меняют parent и список, защищаются от повторного parent и уведомляют
глобальный scene manager; полная семантика последних уведомлений ещё не названа.
Destructor последовательно:

1. удаляет/release-ит все collision entries;
2. отсоединяется от parent;
3. отсоединяет всех детей;
4. снимает `+0x3C` с глобальной регистрации;
5. уничтожает оба контейнера и `spNamedObject`.

### Связи и оставшееся неизвестным

Открыты: original header/TU и method names, остальные dirty/cache masks,
семантика dirty bits `2/4`, frame caller и полная семантика callbacks,
bounds aggregation, recursive name lookup,
полное portable scene/derived virtual world подключение, collision ownership API и serializer reader behavior
для legacy отсутствующего `esfNodeIsAnimated`.

### Поля serializer

| Field | Имя executable | Payload | Default / правило записи |
| ---: | --- | --- | --- |
| 0 | `esfNodePosition` | `Vector3` | `(0,0,0)`, default опускается |
| 1 | `esfNodeRotation` | quaternion XYZW | identity, default опускается |
| 2 | `esfNodeScale` | `Vector3` | `(1,1,1)`, default опускается |
| 3 | `esfNodeIsBone` | byte boolean | `false`, записывается только `true` |
| 4 | `esfNodeIsStatic` | byte boolean | `false`, записывается только `true` |
| 5 | `esfNodeChild` | object relationship | повторяемое поле |
| 6 | `esfNodeBillboardAxis` | `UInt32` enum | `0`; writer поддерживает `1/2` |
| 7 | `esfNodeCollision` | object relationship | повторяемое поле |
| 8 | `esfNodeIsAnimated` | byte boolean | текущий writer пишет явно |

Field 6 подтверждён обоими executable, но у объектов точного типа `spNode` не
встретился. Названия двух осей пока не восстановлены, поэтому Viewer честно
показывает `engine axis 1/2`, а не подставляет догадку X/Y.

### Child и collision relationships

Обычная форма relationship:

```text
UInt32 objectId
UInt32 inlineSerializedSize
byte   inlineSboo[inlineSerializedSize]
```

`inlineSerializedSize == 0` означает ссылку на уже сериализованный объект. При
ненулевом размере inline-данные начинаются с class ID и `SBOO`; размер точно
совпадает с `SerializedSize` соответствующей строки object directory.

| ID-only, 4 байта | Sized reference, 8 байт | Inline object |
| ---: | ---: | ---: |
| 7 | 1 445 | 30 469 |
| 7 | 1 445 | 30 469 |
| 0 | 1 584 | 24 500 |

Все 432 field 7 имеют inline-форму и разрешаются в `spCollisionInfo`. Field 5
ссылается на классы, являющиеся node-ролевыми объектами движка:

Object-directory `ParentIndex` по-прежнему означает физическое владение
serializer-интервалом. Логический parent/child граф строится только по field 5;
теперь в него входят inline, sized-reference и ID-only формы.

### Viewer и база

`SmoNodeDecoder` применяет подтверждённые defaults, декодирует quaternion,
флаги, billboard enum и обе relationship-формы. Read-only inspector показывает
каждое поле. `SmoNodeHierarchy` больше не теряет четыре-байтовые PC-ссылки.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spNode
```

Безопасная мутация остаётся открытой: изменение transform/flags или состава
отношений нужно отдельно проверить в native runtime с пересборкой inline sizes и
object directory.
