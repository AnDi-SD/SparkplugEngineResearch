# Guard speed and vision properties in UpsieDaisyGuardAIBehavior (PC)

The PC property publisher `0x0056D3F0` binds four binary32 fields for `wxUpsieDaisyGuardAIBehavior`. The class's registration and RTTI identify the receiver with primary `0x007001E0`.

| PC property label | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Walk Speed (cm/s)` | `+0x164` | `0x00555EC0` | `0x00567EB0` |
| `Turn Speed (rad/s)` | `+0x188` | `0x004DAE40` | `0x0056C530` |
| `Cos of opening vision opening [0,1]` | `+0x194` | `0x004FC880` | `0x0056C540` |
| `Vision cone sqr length (cm^2)` | `+0x198` | `0x004FC890` | `0x0056C550` |

Each getter executes `FLD` on its selected field and returns the extended value in x87 ST0. Each setter copies the four argument bytes into its selected field and returns with `RET 4`. The cosine label includes a range, and the setter copies the supplied value without a range check or conversion.

The analysis profile accepts a 412-byte captured prefix with the selected primary. It admits finite normal binary32 values and either signed zero in all four fields and in the setter argument. The getter returns the ten little-endian bytes of the extended value. The setter stages a copy of the prefix, updates only the selected field, and supports aliasing the input and output prefix. Refusal leaves output unchanged. The separate 392-byte [Guard Num profile](upsie-daisy-guard-number.md) retains its raw DWORD behavior.

The historical factory allocated 488 bytes. The 412-byte prefix reaches the end of the selected fields. Construction, native layout, lifetime, vision calculation, movement, production callers, PS2, gameplay and whole-class behavior remain open. Subnormals and non-finite values remain outside the captured analysis profile.
