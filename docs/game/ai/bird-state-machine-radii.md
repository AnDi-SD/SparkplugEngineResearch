# Movement and safety radii in BirdStateMachine (PC)

PC registration and RTTI identify `wxBirdStateMachine` with class ID `0x3CE04BD0` and primary `0x006FA878`. The publisher at `0x00529EA0` binds two adjacent fields in the order below.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Movement Radius` | `+0x25C` | `0x00529010` | `0x0052E370` |
| `Safety Radius` | `+0x260` | `0x00529020` | `0x00529030` |

Each getter loads the selected binary32 field with `FLD` and returns. The qualified call starts with an empty x87 stack and control word `0x037F`. Its result is the full ten physical bytes of `ST(0)`. Each setter copies the DWORD at `[ESP+4]` to the selected field and returns with `RET 4`. The original executes the store even if the word already matches.

The analytical profile takes a 612-byte captured prefix with this primary. The standalone setter copies any raw DWORD and preserves every other byte. Each getter admits finite normal binary32 values and either signed zero for its selected word. A composed read/write/read requires both the original selected word and the new word to meet that condition, and returns both x87 values and the full after-prefix. A refused operation preserves output.

The historical factory allocated 636 bytes. The selected prefix ends immediately after `Safety Radius`. Exceptional x87 behavior, radius units and gameplay effects, publisher execution, object lifetime and PS2 remain open.
