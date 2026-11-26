# Hole fields in IceWormAIBehavior (PC)

The PC property publisher `0x005635D0` binds two DWORD fields for `wxIceWormAIBehavior`. Registration and RTTI identify the receiver with primary `0x006FF630`.

| PC property label | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Holes Frequency (ms)` | `+0x1B4` | `0x00561020` | `0x00563240` |
| `Holes Offset (ms)` | `+0x1B8` | `0x0054F0B0` | `0x00563250` |

Each getter loads its DWORD into EAX. Each setter copies the four argument bytes and returns with `RET 4`. The selected field preserves all bit patterns and can be written with its previous value.

The analytical profile takes a 444-byte captured prefix with this primary. It reads a selected word, stages a write, or composes read/write/read. Staging preserves all other bytes and supports aliasing the input and setter output. An unsupported profile, primary or selector leaves output unchanged.

The historical factory allocated 456 bytes. The 444-byte prefix reaches the end of the selected fields. Constructor defaults, numeric signedness, timer behavior, other properties, native layout, lifecycle, PS2 and gameplay remain open.
