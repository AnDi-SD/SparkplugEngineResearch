# `spTemplateManager`: ref-counted template registry

Статус: common identity/base, factory/blank clone, singleton, PS2 exact `0x24`
allocation, PC observed `0x24` prefix, registry lookup/add/clear and native
reference-count transitions are confirmed. The larger `spTemplateObject`
and serializer pipeline remain separate classes.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x04BB6643` | `0x04BB6643` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration / initializer | `0x007600F0 / 0x006D38F0` | `0x004A8AA0 / 0x004821D0` |
| Factory | protected `.rld` `0x00462B50` | `0x00156C50` |
| Main/support vtables | `0x006E77E0 / 0x006E77D8` | `0x0048E0D0 / 0x0048E0F4` |
| Singleton storage | `0x0075DC64` | `0x0049F9A8` |
| Extent | observed through `+0x23` | exact `0x24` |

PS2 factory requests `0x24` bytes and initializes a list at `+0x14` plus an
unknown zero word at `+0x20`. Its list uses count `+0x14` and inline sentinel
links `+0x18/+0x1C`; PC again has allocator state `+0x14`, heap sentinel
`+0x18`, count `+0x1C` and the same unresolved `+0x20`. The PC factory remains
protected, so `0x24` is an observed prefix there rather than a direct sizeof.

Manager-local operations agree:

- find: PC `0x004628F0`, PS2 `0x00156730`; both normalize the requested string,
  walk in insertion order and compare it to a string inside each instance;
- clear: PC `0x004628A0`, PS2 `0x00156870`; optional release decrements the
  16-bit counter at instance `+0x08`, destroys at zero, then clears the list;
- add: PS2 `0x001569B0`; appends and increments that same counter. The matching
  PC operation is behaviorally visible but its protected entry is not named;
- clone: PC `0x00462BB0`, PS2 `0x00156B50`; creates a blank manager and invokes
  root no-payload copy, so registered templates are runtime-only.

The nearby PS2 registration getter `0x00156CF0` starts `spTemplateObject`
(`0x014E1394`, direct base `spNamedObject`), and strings prove
`spTemplateObject.cpp` and `spTemplateSerializer.cpp` exist. The preceding
registration is the independent RTTI class `spTemplateInstance`
(`0x1F6A7DA5`, direct base `spNamedObject`), not an internal manager field.
Its functions are not attributed to the manager merely because they are
contiguous in the executable.

The portable `spTemplateManager` uses `shared_ptr<spNamedObject>` as an explicit
temporary seam for proven name lookup and reference lifetime. It does not claim
that `spNamedObject` is the original concrete `spTemplateObject` declaration.

Open: original manager TU/header/method names, direct PC allocation, `+0x20`,
exact normalization rules, the PC add entry, 16-bit reference owner contract,
and the complete template-object/serializer graph.
