# spTemplateManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTemplateManager](../../../Sparkplug/Code/Sparkplug/spTemplateManager.h), [spTemplateObject](../../../Sparkplug/Code/Sparkplug/spTemplateObject.h).

Статус: common identity/base, factory/blank clone, singleton, PS2 exact `0x24`
allocation, PC observed `0x24` prefix, registry lookup/add/clear and native
reference-count transitions are confirmed. The larger `spTemplateObject`
and serializer pipeline remain separate classes.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0x04BB6643` | `0x04BB6643` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
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

Open: original manager TU/header/method names, direct PC allocation, `+0x20`,
exact normalization rules, the PC add entry, 16-bit reference owner contract,
and the complete template-object/serializer graph.
