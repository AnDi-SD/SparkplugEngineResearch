# wxPullLeverState

Class ID `5B00514B`, physical and registration base
[wxCharacterState](wx-character-state.md). PC/PS2 native size is `3C`, selector
is `1F`. No own fields extend the base. Clone constructs fresh defaults;
inherited Copy leaves runtime fields unchanged. The [portable source](../../../Winx/Code/wxPullLeverState.h)
restores the own PC hooks with an explicit external graph and service boundary.

| Hook | PC | PS2 |
| --- | ---: | ---: |
| Entry `1C` | `51ECD0` | `2D22E0` |
| Exit `20` | `51EDF0` | `2D2220` |
| Hook `2C` | `51EC80` | `2D2550` |
| Permission `34` | `523770` | `2D25B0` |

Entry rewrites the request as `(key & F0000980) | 980`, looks up animation,
always queues it in mode zero with interrupt and stores pending. Null and
unchanged handles are also queued. It then directly clears owner action-control
word `4`, rereads owner entity and sends
`[272E,0,0,6,sourceState,0,entity,0]` through the filtered service. The subsequent
owner reread sets entity-controller byte `178` on PC (`184` on PS2) to `1`.

Entry obtains the global player and clears its controller through PC service
`4D96A0`. PS2 inlines zero stores to controller words `194/198/19C`,
`1A0/1A4/1A8`, `1BC/1C0/1C4`, `1C8/1CC/1D0` and `1D4/1D8/1DC`.
Their complete source-level field names remain unknown. The portable PC service
adapter does not claim to implement this foreign controller or its PS2 ABI.

Finally, entry reads receiver field `2B4` of the global context and sends
`[2737,0,0,0,sourceState,0,flagWord,0]` when receiver is nonnull. Only flagWord
low byte is initialized to `1`; upper 24 bits are unwritten native stack padding.
Exit clears the owner entity-controller byte and sends the same typed packet
with low byte `0`. It preserves pending and does not call base release or update.

Hook `2C` releases pending through the base helper, then rereads owner and
selects PC service `4FACE0` for nonzero unsigned word `owner+218`, or `4FAD70`
for zero. PS2 uses `owner+224` and services `2B4010/2B3F70`. The source-level
meaning of this branch remains open. Permission always consumes the completion
query, including null pending. Update/event are inherited empty hooks;
hook `38` returns false. Reset retains the base behavior.

The [required host](../../../Winx/Analysis/Host/wxPullLeverStateHost.h) supplies
borrowed owner/entity/player/receiver graphs, animation and notification services.
It preserves the known flag byte without asserting padding values. The
[tests](../../../Winx/Tests/wxPullLeverStateTests.cpp) include a complete
entry → exit → release cycle, pending preservation, unsigned branching and
owner reread after the release callback. Native ABI, missing external services
and a complete connected game runtime remain outside this component.
