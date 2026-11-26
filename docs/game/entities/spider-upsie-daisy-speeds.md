# Walk Speed and Turn Speed in SpiderUpsieDaisyAIBehavior (PC)

The PC own publisher for `wxSpiderUpsieDaisyAIBehavior` binds two floating-point properties to single-field callbacks. The publisher and the class's registration and RTTI identify the receiver. The Turn Speed callback addresses are shared with another class.

| PC property label | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Walk Speed (cm/s)` | `+0x13C` | `0x00510110` | `0x004E2D60` |
| `Turn Speed (rads/s)` | `+0x140` | `0x004F2B00` | `0x0056A600` |

Each getter executes `FLD` on the selected binary32 value and returns it in x87 ST0. Each setter copies the four raw argument bytes to its selected field and returns with `RET 4`.

The analysis profile uses a separate 324-byte captured prefix and checks primary `0x00700098`. It admits finite normal binary32 values and either signed zero in both fields and in the setter argument. The getter returns the ten little-endian bytes of the corresponding extended value, including the sign of zero. The setter stages a copy of the prefix, updates the selected field and preserves all other bytes. The output may alias the input prefix for a setter. Refused profiles, primaries, selectors and values preserve output. The 312-byte [Spider Num profile](spider-upsie-daisy-number.md) continues to cover its raw DWORD independently.

The historical factory allocated 332 bytes. The 324-byte prefix reaches the end of the Turn Speed field. Constructor, native object layout, object lifetime, production callers, AI movement, PS2 and gameplay remain open. Non-finite and subnormal conversion behavior remain outside this profile.
