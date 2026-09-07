# `spMaterialDataSerializer`: concrete material-data serializer

PC checkpoint12: [actual3C factory and secondary42F670/4B0CF0](native-pc-material-scalar.md).
Five scalar/pass-only native/source rows match exactly. This is not a full
material-layer or whole-file loader claim; inherited codec remains shared.

PC checkpoint13 resolves header42F4C0: reads eight bytes, ignores both words,
creates **spDXMaterial**, not spMaterialData. TargetID6160348B is the wire
registration, not a guarantee of runtime class. The complete small Model
graph, standard layers and save are covered in
[PC standard graph](native-pc-material-standard-graph.md).

Статус: RTTI, lifetime, размер, target и тонкие read/write-обёртки подтверждены
независимо в PC и PS2. Собственной material grammar у класса нет: она наследуется от
`spMaterialSerializer`.

Полный путь исходного translation unit в исполняемых файлах пока не найден. Размещение в
`Code/Sparkplug` следует подтверждённому base-классу; это отдельно помечено в исходнике.

## Идентичность и ABI

Class ID — `0x0B251467`, прямой зарегистрированный и C++ base —
`spMaterialSerializer` (`0x2A14745F`), target — `0x6160348B` (`spMaterialData`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E648` | `0x004AB0B0` |
| Initializer | `0x006D2CF0` | `0x00483D50` |
| Factory | `0x0042F690` (protected entry) | `0x001A54F0` |
| Allocation | observed `0x3C` | exact `0x3C` |
| Primary vtable | `0x006DE5C8` | header `0x004901D0` |
| Secondary vtable | `0x006EFD68` | header `0x004901F4` |

PS2 factory выделяет ровно `0x3C` байта. Конструктор/деструктор меняют vptr поверх
`spMaterialSerializer`, а новых members после его embedded data-block helper не
инициализируют. PC factory находится в защищённой секции, поэтому его размер отмечен как
наблюдаемый, а не как independently exact.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0042F630` | `0x001A52C0` |
| write wrapper | inherited/common path | `0x001A52D0` |
| read wrapper | inherited/common path | `0x001A52E0` |
| signature/load helper | `0x0042F4C0` | `0x001A5310` |
| target class ID | `0x0042F660` | `0x001A5390` |
| destructor | `0x0042F640` | `0x001A53A0` |
| blank clone | `0x0042F700` | `0x001A5410` |
| deleting destructor | `0x0042F750` | ABI-combined |

PS2 write wrapper передаёт управление общему writer `spMaterialSerializer` по
`0x00193060`. Reader сначала отклоняет нулевой target, затем вызывает общий reader по
`0x00193EB0`. Secondary thunks находятся по `0x001A5570` и `0x001A5560`; index thunk
остаётся у общего material serializer (`0x00195E40`). Это подтверждает, что отдельной
грамматики и собственных resource relationships у leaf-класса нет.

## Восстановленная граница

Portable-класс восстанавливает только доказанные свойства:

- точную RTTI-цепочку и фабрику;
- concrete blank clone;
- target ID;
- нулевую read-проверку;
- наследование стандартного material plan от `spMaterialSerializer`.

Он намеренно не копирует внутренний stream codec. Target-класс теперь восстановлен
отдельно вместе с concrete color/power layout и необычной blank-clone семантикой:
см. [`native-class-sp-material-runtime.md`](native-class-sp-material-runtime.md).
Планировщик базового класса по-прежнему отклоняет нестандартные layer classes,
так что thin leaf не расширяет доказательную границу молча.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты фиксируют direct
base/factory, target, null-target guard, наследуемый порядок material fields, concrete
clone и раздельные PC/PS2 ABI-константы.

## Открытые вопросы

1. Original header и точный путь translation unit.
2. Имя secondary serializer interface и точные сигнатуры read/write/load hooks.
3. Остальные allocation/error варианты (actual PC3C factory уже проверена).
4. Semantics статуса reader-а и поведение при частично прочитанном datablock.
5. Контролируемая in-game проверка изменения material payload.
