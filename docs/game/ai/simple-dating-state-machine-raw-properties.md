# Raw properties of SimpleDatingStateMachine (PC)

PC registration identifies `wxSimpleDatingStateMachine` as `0x0DDA040E` with primary `0x006FB340`. The publisher at `0x00532EE0` binds these properties in the listed order.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Used for Bloom(true), Sky(false)` | `+0x22C` | `0x00532AF0` | `0x00532B00` |
| `Date Number` | `+0x230` | `0x00532B10` | `0x00532B20` |

The first getter returns the raw byte in `AL`. Its setter takes the low byte of the argument at `[ESP+4]` and stores it at `+0x22C`. The Date Number getter returns the full DWORD in `EAX`, and its setter copies all four argument bytes to `+0x230`. Each setter consumes the four-byte stack argument and preserves every byte outside its selected field.

The captured profile takes a 564-byte prefix with the recorded primary. A composed operation reads the selected value, stores the argument and returns the original value, the complete updated prefix and the value read back after the store. The Bloom option uses a raw byte and Date Number uses a raw little-endian DWORD.
