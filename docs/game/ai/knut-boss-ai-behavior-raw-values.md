# Raw hit and attack-time fields in KnutBossAIBehavior (PC)

PC registration and RTTI identify `wxKnutBossAIBehavior` with class ID `0x0D367CE3` and primary `0x006FF490`. Its publisher at `0x00562A40` binds four DWORD fields in the order below.

| Property | Receiver offset | Getter | Setter |
| --- | --- | --- | --- |
| `Nb Hit for losing glasses` | `+0x1B0` | `0x0054ED20` | `0x00563230` |
| `Nb Hit for falling` | `+0x1B4` | `0x00561020` | `0x00563240` |
| `Min Time between attack` | `+0x1C8` | `0x00554180` | `0x00562300` |
| `Max Time between attack` | `+0x1CC` | `0x00554190` | `0x00562310` |

Each getter loads the selected field into EAX and returns. Each setter loads a DWORD from `[ESP+4]`, stores every bit in the selected field and returns with `RET 4`. The four callbacks for writing preserve the intervening bytes `+0x1B8` through `+0x1C7`, the other three fields and all unrelated bytes.

The analytical profile accepts a 464-byte captured prefix with the Knut Boss primary. A composed read/write/read returns both raw values and the full after-prefix. A setter supports aliasing the input and output. A refused profile, selector or primary preserves output.

The historical factory allocated 564 bytes. The captured fields held raw words `0x0000000A`, `0x00000003`, `0x000000C8` and `0x000003E8` in publication order. Hit thresholds, timing units, signedness, normalization, attack decisions, lifetime, PS2 and gameplay remain open.
