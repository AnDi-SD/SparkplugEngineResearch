# Shield time and blast damage fields in WinxHelpAIBehavior (PC)

PC registration and RTTI identify `wxWinxHelpAIBehavior` with class ID `0x423845B1` and primary `0x006FFEE0`. Its publisher at `0x00569C10` binds the fields below in order.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Tecna Shield lasting time(ms)` | `+0x1C4` | `0x00554170` | `0x005640A0` |
| `Musa Blast Damage` | `+0x1C8` | `0x00554180` | `0x00562300` |
| `Stella Blast Damage` | `+0x1CC` | `0x00554190` | `0x00562310` |

A getter loads the selected DWORD into EAX and returns. A setter loads the DWORD at `[ESP+4]`, stores all four bytes at the selected offset and returns with `RET 4`. The class publisher establishes the field identities where other classes share these callback addresses.

The analytical profile accepts a 464-byte captured prefix with the Winx Help primary. A composed read/write/read returns the raw before and after values and the complete after-prefix. A setter preserves every other byte and supports aliased input and output. A refused profile, selector or primary leaves output intact.

The historical factory allocated 464 bytes. The captured fields held raw words `0x00003A98`, `0x0000000F` and `0x0000000F` in publication order. The first label includes `(ms)`, and its callbacks transfer the word without conversion. Timing and damage consumption, numeric representation in gameplay, clamping, lifecycle and PS2 remain open.
