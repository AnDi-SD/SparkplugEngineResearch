# spTemplateObject

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTemplateObject](../../../Sparkplug/Code/Sparkplug/spTemplateObject.h).

Статус: exact source path, identity/base, exact `0x1B0` allocation and layout,
constructor/destructor, four-way state dispatch, resource cleanup and the native
clone subset are confirmed on PC and PS2. Full serializer semantics and the
concrete meanings of the four state values remain open.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0x014E1394` | `0x014E1394` |
| Base | `spNamedObject / 0x44DE07FD` | `spNamedObject / 0x44DE07FD` |
| State dispatch / cleanup | `0x005FDA40 / 0x005FD790` | `0x00156D00 / 0x00156F50` |
| Exact allocation | `0x1B0` | `0x1B0` |

The PC binary preserves the exact diagnostic path
`Z:\Sparkplug\Code\Sparkplug\spTemplateObject.cpp`. Both factories directly
request `0x1B0`, and both constructors establish the same field boundaries:

| Offset | Size | Confirmed behavior |
| ---: | ---: | --- |
| `+0x14` | `0x08` | embedded two-word object initialized to zero |
| `+0x1C` | `0x04` | nullable loaded/runtime object; cleanup is state-dependent |
| `+0x20` | `0x04` | serializer-supplied state, accepted dispatch values `0..3` |
| `+0x24` | `0x04` | nullable stream/resource interface released before dispatch cleanup |
| `+0x28` | `0x04` | initialized zero, role unknown |
| `+0x2C` | `0x100` | inline resource path buffer |
| `+0x12C` | `0x04` | initialized zero and copied |
| `+0x130` | `0x04` | initialized `-1` and copied |
| `+0x134` | `0x40` | zero-initialized opaque block, copied with `+0x130` |
| `+0x174` | `0x3C` | 15 constructor defaults loaded from platform globals; not copied |

Crucially, neither constructor initializes `+0x20` nor the path at `+0x2C`.
They are a serializer contract, not valid default state. The portable class
therefore represents their absence with `optional` and validates a four-value
state and a maximum 255-character path instead of silently inventing native
defaults.

The dispatcher agrees instruction-for-instruction at the contract level:

The surviving message is   `Failed to create entity/behavior from its property stream`.

Native copy is unusually selective. PC `0x005FD720` and PS2 `0x00157070` copy
`+0x1C`, state, path, `+0x12C`, and `+0x130..+0x173`; they do not call the
`spNamedObject` copy routine. Consequently a clone does **not** inherit the
descriptor name. Constructor-only `+0x14`, `+0x24/+0x28`, and `+0x174..+0x1AF`
remain fresh. The reconstruction and tests preserve this non-obvious split.

The adjacent class is now independently identified as `spTemplateSerializer`
(`0x41577707`, direct `spBaseObject`), with exact PC source string
`Z:\Sparkplug\Code\Sparkplug\spTemplateSerializer.cpp`. Its parsing routines
populate this object, but are not folded into `spTemplateObject`.

Open: original member/method/enum names, meanings of states `0..3`, types and
ownership at `+0x14/+0x1C/+0x24/+0x28`, semantic meaning of both opaque blocks
and 15 defaults, and the complete serializer-to-template-to-entity pipeline.
