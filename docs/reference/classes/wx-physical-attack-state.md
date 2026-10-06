# wxPhysicalAttackState

Class ID `78781786`, physical and registration base
[wxCharacterState](wx-character-state.md). PC and PS2 native size is `3C`,
selector is `13`. The [portable source](../../../Winx/Code/wxPhysicalAttackState.h)
keeps the native identity and recovered hooks; its representation does not claim
native ABI compatibility. No own fields extend the base. Clone constructs fresh
base defaults; inherited root Copy does not transfer pending or transition flags.

| Hook | PC | PS2 |
| --- | ---: | ---: |
| Entry `1C` | `5176C0` | `2D0B90` |
| Update `30` | `523690` | `2D0CC0` |
| Permission `34` | `523770` | `2D0E30` |
| Hook `38` | `5175E0` | `2D0E10` |
| Event `3C` | `517610` | `2D0CD0` |

Entry clears the direct owner action-control word `4` **before** rewriting the
request as `key & F007FF8F`. It rereads owner for animation lookup, always queues
the resulting handle in mode zero with interrupt, and stores pending. Null and
unchanged handles are also queued. No old-pending release or virtual update is
called by this entry. It then rereads owner entity (`owner+124` on PC,
`owner+130` on PS2) and sends filtered packet
`[2731,0,0,6,sourceState,0,entity,0]`; entity may be null. Filter meaning is open.

Update only clears the direct action-control word. Permission always invokes the
consuming completion query, including null pending. Hook `38` returns true exactly
for code `9`. Other hooks retain the base behavior.

Event compares complete case-sensitive names `event_impact_begin` and
`event_impact_end`. Either match reads borrowed receiver `owner+24`. A nonnull
receiver gets packet `[271F,0,0,0,sourceState,0,foot_left,flagWord]` through its
virtual notification slot. Payload zero points to the string `foot_left`;
payload one has known low byte `1` for begin or `0` for end. Its upper 24 bits
are unwritten native stack padding and have no asserted value. A null receiver
or another event name sends no packet.

The [required host](../../../Winx/Analysis/Host/wxPhysicalAttackStateHost.h)
provides borrowed event/entity/receiver graphs, animation services and delivery.
Its reuse of the OpenGate filtered-message adapter is an analytical choice.
External receiver and animation implementations, native allocation and a complete
connected game runtime remain outside this component.
