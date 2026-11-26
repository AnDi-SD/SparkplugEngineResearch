# Wait For Signal and Follow floor in UpsieDaisyGuardAIBehavior (PC)

The PC own publisher `0x0056D3F0` binds two raw byte properties of `wxUpsieDaisyGuardAIBehavior`. Registration and RTTI link the publisher to the class with primary `0x007001E0`.

| PC property label | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Wait For Signal (elevator)` | `+0x1D4` | `0x0056C560` | `0x0056C570` |
| `Follow floor ?` | `+0x1D5` | `0x0056C580` | `0x0056C590` |

A getter loads one receiver byte into AL and returns. A setter loads one byte from `[ESP+4]` into AL, stores it in the selected field and returns with `RET 4`. The four-byte argument slot and the one-byte read have distinct widths. Both callbacks preserve the upper 24 bits of entry EAX. Every low-byte pattern stays distinct, including `0x02`, `0x80` and `0xFF`.

The raw byte facade takes a separate 470-byte captured prefix with the selected primary. Its getter returns the chosen byte. Its setter stages a prefix copy and stores the low byte of the supplied `uint32_t` argument word. It preserves every other byte and supports input/output aliasing. Refusal preserves output. The original register observations characterize the selected instruction bodies. The value API exposes raw bytes, and the complete native signature remains open.

The previous [Guard Num profile](upsie-daisy-guard-number.md) uses 392 bytes and the [scalar profile](upsie-daisy-guard-scalars.md) uses 412 bytes. The 470-byte extent includes both selected byte fields. The historical factory allocated 488 bytes. Native size, layout, lifetime, Boolean interpretation, floor following, elevator dispatch, production callers, PS2 and gameplay remain open.
