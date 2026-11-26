# Four scalar properties of GhostlySpiritStateMachine (PC)

PC registration and RTTI identify `wxGhostlySpiritStateMachine` with class ID `0x88760331` and primary `0x006FAAD8`. The publisher at `0x0052B3D0` binds these properties in the order shown.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Trigger Radius` | `+0x230` | `0x005373A0` | `0x00532B20` |
| `Movement Radius` | `+0x234` | `0x004FD7C0` | `0x0052AE90` |
| `Height Range` | `+0x238` | `0x004FD7D0` | `0x0052AEA0` |
| `Movement Speed` | `+0x23C` | `0x0052AEB0` | `0x0052AEC0` |

Each getter loads the selected binary32 field with `FLD`. The captured profile starts with an empty x87 stack and control word `0x037F`, and measures all ten physical bytes of the returned `ST(0)`. Each setter copies one DWORD from `[ESP+4]` to the selected field and returns with `RET 4`. The original executes the store even when the DWORD already matches.

The analytical profile takes a 576-byte captured prefix with the recorded primary. A standalone setter accepts any raw DWORD and preserves every other byte. A getter accepts finite normal binary32 values and either signed zero in its selected field. A composed read, write and read applies the same value restriction to the argument and returns both physical x87 values and the whole updated prefix. A refused operation preserves its output.

The historical factory allocated 596 bytes. The capture ends immediately after `Movement Speed`. Units, gameplay effects, exceptional x87 values, publisher execution, object lifetime and PS2 behavior remain open.
