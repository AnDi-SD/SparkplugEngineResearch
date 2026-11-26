# Time fields in TrollAIBehavior (PC)

The PC property publisher `0x00565700` binds two DWORD fields for `wxTrollAIBehavior`. Registration and RTTI identify the receiver with primary `0x006FF9C0`. The minimum time callback addresses are also used by another class.

| PC property label | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Min time between attacks` | `+0x1B8` | `0x0054F0B0` | `0x00563250` |
| `Max time between attacks` | `+0x1BC` | `0x00561030` | `0x00564080` |

Each getter loads its DWORD into EAX. Each setter copies all four argument bytes and returns with `RET 4`. Both operations preserve raw bit patterns. A setter writes the selected value even when it already equals the stored word.

The analytical profile takes a 448-byte captured prefix with this primary. It reads a selected word, stages a write, or composes read/write/read. Staging preserves every other byte and supports aliasing the input and setter output. An unsupported profile, primary or selector leaves output unchanged.

The historical factory allocated 452 bytes. The 448-byte prefix reaches the end of the selected fields. Constructor defaults, numeric signedness, units, ordering between the two values, timer use, native layout, other methods, PS2 and gameplay remain open.
