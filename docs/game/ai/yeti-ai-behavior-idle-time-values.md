# Idle time fields in YetiAIBehavior (PC)

PC registration and RTTI identify `wxYetiAIBehavior` with class ID `0x26277D61` and primary `0x006FF790`. The class publisher at `0x005643A0` binds the two idle-time labels to its first two descriptor records.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Min Idle time` | `+0x1B8` | `0x0054F0B0` | `0x00563250` |
| `Max Idle time` | `+0x1BC` | `0x00561030` | `0x00564080` |

Each getter loads the selected DWORD into EAX. Each setter loads a full DWORD from `[ESP+4]`, writes it to the selected field and returns with `RET 4`. Both setters preserve every bit of the supplied word and leave the adjacent field intact. The publisher establishes the Yeti property names even when another publisher reuses the same callback addresses.

The PC analytical profile takes a 448-byte captured prefix with this primary. A composed read/write/read returns both raw words and the full after-prefix. The setter supports an aliased input and output. A refused profile, selector or primary preserves output.

The historical factory allocated 460 bytes and the captured fields held raw words `0x000000C8` and `0x000003E8`. The selected prefix ends after the maximum field. Time units, range semantics, idle decisions, the SnowBlow and tooth fields, PS2, lifetime and gameplay remain open.
