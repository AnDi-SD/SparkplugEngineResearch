# Burst fields in StarEmitterTarget (PC)

The PC property publisher `0x00512520` binds two DWORD fields for `wxStarEmitterTarget`. Registration and RTTI identify the receiver with primary `0x006F8088`.

| PC property label | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Burst Qty` | `+0x154` | `0x0053D050` | `0x0054FDC0` |
| `Burst Star Value` | `+0x158` | `0x00540D30` | `0x00540D20` |

Each getter loads its DWORD into EAX. Each setter copies all four argument bytes to the selected field and returns with `RET 4`. High-bit patterns and equal-value stores use the same operation as other DWORDs.

The analytical profile takes a 348-byte captured prefix with this primary. It can read a selected word, stage one write, or compose read/write/read on the supplied state. Staging preserves all other bytes and supports aliasing the input and setter output. An unsupported profile, primary or selector leaves output unchanged.

The historical factory allocated 356 bytes. The 348-byte prefix reaches the end of the selected fields. The observed property labels leave numeric signedness, ranges, constructor defaults, native layout, spawning, gameplay and PS2 behavior open.
