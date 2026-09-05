# `spPS2MaterialDataSerializer`: PS2 material marker

Статус: RTTI/lifetime, direct base, storage-free derived ABI и наследуемый material
contract подтверждены в PC и PS2. Несмотря на имя, класс присутствует в обеих сборках.

Class ID — `0x69327633`; прямой base — `spMaterialSerializer` (`0x2A14745F`), не
`spMaterialDataSerializer`. Независимого target-hook нет; write/index/read и layer
helpers остаются общими.

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E5E8` | `0x004AB110` |
| Initializer | `0x006D2CC0` | `0x00483D90` |
| Factory | `0x0042F550` (protected entry) | `0x001A5760` |
| Allocation | observed `0x3C` | exact `0x3C` |
| Destructor | `0x0042F530` | `0x001A5610` |
| Blank clone | `0x0042F5C0` | `0x001A5680` |
| Registration getter | `0x0042F520` | `0x001A5580` |
| Signature/load helper | `0x0042F4C0` | `0x001A5590` |
| Primary vtable | `0x006DE574` | header `0x00490230` |
| Secondary vtable | `0x006DE514` | header `0x00490254` |

PS2 factory выделяет `0x3C`, вызывает constructor общего material serializer и меняет
только vptr. Его secondary table наследует thunks
`0x00195E50/0x00195E40/0x00195E30`; новых relationships или fields класс не вводит.
Совпадение не сводится автоматически с DX ABI: адреса lifetime/RTTI/vtable хранятся
раздельно.

Portable-класс восстанавливает RTTI и concrete blank clone. Стандартная material grammar
приходит из base. PS2 runtime-лист `spPS2Material` теперь отдельно подтверждён до
exact `0xD0`, color/power layout, deep clone и pass-update consumer; сам механизм
выбора/conversion serializer-а всё ещё не доказан.

Открыты original header/source path, интерфейсные имена/signatures, прямой PC allocation,
platform selection/conversion в `spPS2Material` и in-game mutation test.
