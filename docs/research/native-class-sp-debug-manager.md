# `spDebugManager`: engine debug state and renderer helpers

Статус: common PC/PS2 identity, direct base, factory/clone, exact `0x38`
allocation, singleton, layouts and a small state slice are confirmed. It is
not the base of game class `wxDebugManager`: that independent registration also
derives directly from `spBaseObject`.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x37054B40` | `0x37054B40` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration | `0x0075DC08` | `0x004AD1B0` |
| Initializer | `0x006D2720` | `0x00484AB0` |
| Factory | `0x0041E380` | `0x001D9DF0` |
| Constructor | protected `.rld` entry `0x0041D3A0` | `0x001D9C30` |
| Primary/support vtables | `0x006DC3F0 / 0x006DC3EC` | `0x00490EF0 / 0x00490F14` |
| Singleton | `0x00755278` | `0x0049F854` |
| Size | `0x38` | `0x38` |

PS2 constructor clears word `+0x14`, twelve bytes `+0x18..+0x23` and five
owned pointers `+0x24..+0x34`. Destructor `0x001D9AD0` releases those five
objects and clears the singleton. PC destructor `0x0041E590` instead releases
two renderer globals, so its `+0x14..+0x37` tail remains opaque in the PC ABI
record even though factory allocation proves the same extent.

PS2 `0x001D9A50` advances the `+0x14` cursor through a 20-entry table, wraps at
20 and skips a sentinel value. The precise table type is not known. Individual
byte flags have direct setters/consumers (for example `0x001D8570` writes
`+0x18`). Clone `0x001D9D30` / PC `0x0041E690` constructs a blank manager and
uses root no-payload copy, so renderer helpers and flags are not copied.

`Sparkplug/Code/Sparkplug/spDebugManager.h/.cpp` therefore implements concrete
RTTI/factory, singleton, bounded 12-flag state, a 20-step analytical cursor and
blank clone. It does not create renderer objects or claim a palette type.
Exact ABI layouts/anchors and tests are included; isolated build passes 2/2.

Open: original TU/header and method names, exact PC tail, identities of the
five PS2 renderer resources/two PC globals, the table/sentinel type and the
meaning of every debug flag.

PC animation lifecycle продолжение независимо исполнило protected constructor
через `spAnimation`. Helper `0x0041D4E0` потребляет PC 20-entry table `0x0073FEE8`
и sentinel `0x0073FEBC`; animation сохраняет результат в `+0x54`. Lazy reference
`0x0075526C` не следует смешивать с собственным singleton `0x00755278`.
Доказательства и границы — [animation lifecycle](native-pc-animation-lifecycle.md).

PC [Model checkpoint10](native-pc-model-render-world.md) подтвердил ещё одного
consumer-а того же `41D4E0`: Renderable ctor сохраняет raw DWORD в `+0x28`,
позднее pre-render переносит его в rendererC194. Прежнее имя renderer-global
float исправлено; это не отдельный вновь найденный renderer manager и не
доказательство исходного имени/типа всей таблицы.
