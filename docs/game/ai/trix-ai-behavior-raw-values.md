# Idle time and IceShield fields in TrixAIBehavior (PC)

PC registration and RTTI identify `wxTrixAIBehavior` with class ID `0x426D268C` and primary `0x007003D0`. Its publisher at `0x005704D0` binds four DWORD fields in the order below.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Min Idle time` | `+0x1B8` | `0x0054F0B0` | `0x00563250` |
| `Max Idle time` | `+0x1BC` | `0x00561030` | `0x00564080` |
| `ICY IceShield time` | `+0x1C0` | `0x005541C0` | `0x00564090` |
| `ICY Nb Hit for IceShield` | `+0x1C4` | `0x00554170` | `0x005640A0` |

Each getter loads the selected field into EAX and returns. Each setter loads a DWORD from `[ESP+4]`, stores every bit in the selected field and returns with `RET 4`. The publisher establishes the Trix labels even when the same callbacks appear in another class's descriptors.

The analytical profile takes a 456-byte captured prefix with the Trix primary. A composed read/write/read returns the before and after values and the full after-prefix. Each setter preserves the other three words and all unrelated bytes and supports aliasing the input and output. A refused profile, selector or primary leaves output intact.

The historical factory allocated 456 bytes. The captured fields held raw words `0x000000C8`, `0x000003E8`, `0x00002710` and `0x00000008` in publication order. Units, signedness, idle decisions, range or counter rules, IceShield effects, lifetime, PS2 and gameplay remain open.
