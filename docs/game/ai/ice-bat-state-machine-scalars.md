# Trigger radius and movement speed in IceBatStateMachine (PC)

PC registration and RTTI identify `wxIceBatStateMachine` with class ID `0x448212BB` and primary `0x006FAA58`. Its publisher at `0x0052ACF0` binds the following properties in order.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Trigger Radius` | `+0x22C` | `0x0052A770` | `0x0052A780` |
| `Movement Speed` | `+0x230` | `0x005373A0` | `0x00532B20` |

A getter loads the selected binary32 field with `FLD`. The captured profile starts with an empty x87 stack and control word `0x037F` and returns all ten physical bytes of `ST(0)`. A setter copies the DWORD at `[ESP+4]` to the selected field and returns with `RET 4`. The original still executes the store when the bits match.

The analytical profile uses a 564-byte prefix with the recorded primary. A standalone setter copies any DWORD and preserves all other bytes. Getter inputs and both words in a composed read, write and read admit finite normal binary32 values and either signed zero. A composed result includes the old and new physical x87 values and the complete updated prefix. A refused operation preserves the output.

The historical factory allocated 588 bytes. The capture ends after `Movement Speed`. The field units, gameplay, exceptional x87 values, object lifetime and PS2 behavior remain open.
