# `spGameLevel`: owned template-instance collection

Статус: identity/base, exact `0x2C` allocation, platform list ABI, instance
ownership, constructor/destructor and name-only clone are confirmed on PC and
PS2. Serializer traversal and three trailing resource roles remain open.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x4A45115B` | `0x4A45115B` |
| Base | `spNamedObject / 0x44DE07FD` | `spNamedObject / 0x44DE07FD` |
| Registration / initializer | `0x007663C0 / 0x006D6490` | `0x004A8BC0 / 0x00482290` |
| Factory / constructor | `0x005A0B80 / 0x005A0B20` | `0x001588F0 /` inline |
| Destructor / clone | `0x005A0A20 / 0x005A0BE0` | `0x00158680 / 0x00158810` |
| Registration getter / vtable | `0x005A0AB0 / 0x00703E08` | `0x00158330 / 0x0048E170` |
| Confirmed add path | protected entry not yet named | `0x00158630` |
| Exact allocation | `0x2C` | `0x2C` |

No exact source path for `spGameLevel` itself has yet been recovered. The
related serializer is independently proven by
`Z:\Sparkplug\Code\Sparkplug\spGameLevelSerializer.cpp`; this does not justify
claiming the same translation unit for the level class.

The compact layout is otherwise exact:

- PC uses allocator state `+0x14`, heap sentinel `+0x18`, count `+0x1C`;
- PS2 uses count `+0x14`, inline sentinel links `+0x18/+0x1C`;
- both clear three trailing pointers at `+0x20/+0x24/+0x28`.

Both destructors traverse the list and virtually destroy every non-null item
before dismantling the container. The PS2 serializer proves the element role:
it creates/populates `spTemplateInstance` objects and passes them to
`0x00158630`, which inserts them into the level's `+0x14` collection. The
portable class consequently owns `unique_ptr<spTemplateInstance>` entries;
remove transfers ownership rather than guessing a native delete flag.

PC vtable `0x00703E08` and PS2 vtable `0x0048E170` both reuse
`spNamedObject`'s copy implementation. Clone constructs an empty list and
zero trailing fields, registers the clone pair, then copies only inherited
named-object state. The level name survives; loaded instances and runtime
resources do not. Tests preserve that distinction.

PS2 `0x00158340` iterates all instances and performs per-instance work with a
temporary progress/interpolation callback. `0x00158580` wraps the larger level
deserialize operation. Neither is assigned an original method name yet.

Open: original declaration/TU, list API spellings, exact roles and ownership of
`+0x20/+0x24/+0x28`, PC add/remove entry points, progress callback contract,
and the complete `spGameLevelSerializer` transaction/rollback behavior.
