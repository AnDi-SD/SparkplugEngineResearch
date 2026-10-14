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
| `+0x174` | `0x0C` | placement position P, loaded from platform globals; not copied |
| `+0x180` | `0x24` | placement orientation R, loaded from platform globals; not copied |
| `+0x1A4` | `0x0C` | placement scale S, loaded from platform globals; not copied |

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
retain the destination's values. A factory-created clone starts with fresh
values; copying into an existing descriptor preserves that descriptor's P/R/S
and name. The reconstruction preserves this split.

The tail is consumed by the instance graph: its position, orientation and scale
are copied to a selected `spNode`, with local-transform dirty bit `1` set after
each group. PC blocks beginning at `0x005FBABF`, `0x005FBBE6`, and `0x005FBDA5`
perform this transfer for a nested instance root, a Node, and an actor's Node,
respectively. Parent lookup and attachment happen separately.

PC construction at `0x005FD810` copies P from `0x007600E0`, R from
`0x007600BC`, and S from `0x00740310`. P is zero and S is `(1, 1, 1)`.
R becomes the identity matrix through the original static initializer
`0x006D38E0`, which calls the matrix constructor with mode `1`. Reading the
unstarted PE image gives a zero matrix and does not represent the initialized
game state. The portable descriptor uses these PC defaults. Its analytical
placement API transfers the same three groups onto the common Node
implementation; descriptor selection and instance graph creation remain
separate work.

PS2 construction at `0x001571D0` copies the same tail from P `0x00476F50`,
R `0x00476DF0`, and S `0x00476F60`. These globals are zero in the pristine
ELF before startup. Their runtime initialization must be established separately;
the PC identity and unit-scale defaults are not asserted for PS2 from that data.

The adjacent class is now independently identified as `spTemplateSerializer`
(`0x41577707`, direct `spBaseObject`), with exact PC source string
`Z:\Sparkplug\Code\Sparkplug\spTemplateSerializer.cpp`. Its parsing routines
populate this object, but are not folded into `spTemplateObject`.

Open: original member/method/enum names, meanings of states `0..3`, types and
ownership at `+0x14/+0x1C/+0x24/+0x28`, semantic meaning of both opaque blocks
and the complete serializer-to-template-to-entity pipeline, including all
instance graph state branches and PS2 placement-global initialization.
