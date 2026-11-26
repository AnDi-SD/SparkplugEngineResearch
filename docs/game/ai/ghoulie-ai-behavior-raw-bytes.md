# Gardenia and Jumping fields in GhoulieAIBehavior (PC)

PC registration and RTTI identify `wxGhoulieAIBehavior` with class ID `0x4BBA28D3` and primary `0x006FF338`. The class publisher at `0x00561C70` binds two adjacent byte fields through its two calls to the byte descriptor writer.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Is In Gardenia01` | `+0x1C4` | `0x00561040` | `0x00566AB0` |
| `Jumping Ghoulie` | `+0x1C5` | `0x00561050` | `0x00566AC0` |

Each getter loads the selected field into AL and returns. Each setter loads one byte from `[ESP+4]`, stores it in the selected field and returns with `RET 4`. The argument occupies four stack bytes. Both kinds of callback preserve the upper 24 bits of entry EAX. This register behavior belongs to the native observation. The value API exposes the selected byte.

The PC analytical profile requires a 454-byte captured receiver prefix with this primary. A setter stages the prefix and replaces exactly one byte with the low eight bits of the argument. The adjacent field and every other byte are preserved. A composed read/write/read returns both raw byte values and the full after-prefix. The API supports aliasing the setter input and output. A refused profile, primary or selector leaves the output intact.

The historical factory allocated 492 bytes. The selected prefix ends immediately after `Jumping Ghoulie`. Boolean interpretation, initialization rules, AI decisions, production callers, native signatures, object lifetime, PS2 and gameplay remain open.
