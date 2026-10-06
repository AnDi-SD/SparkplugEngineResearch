# wxGhoulJumpingState

Class ID `37086498`, physical and registration base
[wxCharacterState](wx-character-state.md). PC/PS2 native size is `3C`, selector
is `1`. No own fields extend the base. Clone constructs fresh defaults;
inherited Copy leaves runtime fields unchanged. The [portable source](../../../Winx/Code/wxGhoulJumpingState.h)
implements the own hooks with borrowed external services.

| Hook | PC | PS2 |
| --- | ---: | ---: |
| Entry `1C` | `51ADC0` | `2E4E00` |
| Hook `2C` | `51EC80` | `2E4F50` |
| Permission `34` | `51AD00` | `2E4FD0` |
| Hook `38` | `51AD20` | `2E4FA0` |
| Event `3C` | `51AD60` | `2E5010` |

Entry first sets `(key & FFBF805F) | 200050`. It reads direct owner
action control (`owner+12C` on PC, `+138` on PS2). Nonzero byte `51` sets
`(key & F17FFFFF) | 01000000`. Otherwise, it replaces bits `0F800000` with
`00800000` when float word `4` is at least binary32 `0.2`, or zero when less.
PC x87 also selects `00800000` for unordered comparison. The finite input
branch agrees with PS2; PS2 nonfinite FPU behavior remains unqualified.

Entry resolves the resulting animation and always queues mode zero with
interrupt, including null and unchanged handles. It stores pending after queue.
It does not release old pending or clear action control. The base queue clears
matching completion records and chooses fade selector `2` or `0` by owner
predicate. Permission returns false for code `17` without accessing a host or
consuming records; all other codes call the consuming query, including null
pending.

Hook `38` returns the integer `2` for codes `0` and `3`, and `1` otherwise.
The original meaning of these values is unknown. The portable virtual contract
preserves the integer result.

Event matches exactly `air`, including the terminating zero. PC compares four
bytes against `air\0`; PS2 calls `strcmp`. A match reads
`owner+124 -> entity+12C` on PC (`+130 -> +138` on PS2), writes jump velocity
`(0,375,0)` and enables the controller byte. The velocity words are PC
`1C8/1CC/1D0`, PS2 `1D4/1D8/1DC`; flag is PC `1D4`, PS2 `1E0`.
Other names leave the controller unchanged. This state has no separate jump
flag or end-event branch.

Hook `2C` shares PC body `51EC80` with [wxPullLeverState](wx-pull-lever-state.md).
It releases pending first, rereads owner, then calls PC `4FACE0` when unsigned
word `owner+218` is nonzero or `4FAD70` when zero. PS2 uses `owner+224` and
calls `2B4010/2B3F70`. Source-level meanings of these services remain open.
Exit and cleanup use base pending release; update is empty and reset is inherited.

The [required host](../../../Winx/Analysis/Host/wxGhoulJumpingStateHost.h)
supplies borrowed action/jump control views, event tags, animation services and
owner branches. [Tests](../../../Winx/Tests/wxGhoulJumpingStateTests.cpp)
cover the entry → event → permission → release cycle, exact event matching,
mode selection, raw integer dispatch and owner reread after a release callback.
Native ABI, missing external services and a complete connected game runtime
remain outside this component.
