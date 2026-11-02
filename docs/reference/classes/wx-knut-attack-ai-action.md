# wxKnutAttackAIAction

`wxKnutAttackAIAction` has class ID `70D47C10`, PC extent `0x408`
(1032 bytes) and a physical [`wxAIAction`](wx-ai-action.md) prefix.
Factory `5ACF60` passes its allocated receiver unchanged
to constructor `5AC680`. That constructor calls
`58FF00` on the same receiver and installs primary `708F58`.
Getter `5AC730` returns record `7670D0`.
The [PC layout](../../../Winx/Analysis/PC/wxKnutAttackAIActionAbi.h) records native
placement separately from portable C++ object layout.

## Whole command-flag Exit

Primary cell `2C` selects complete `5ABA70`,
a 135-byte body with 31 instructions and no calls or branches.
It clears command bytes `1D`, `5C`, `20`, `21`, `22`, `23`, `24` in that order.
Before **each** byte store it reads a fresh
`own20 → owner144 → actor130` chain. Only after all 7 stores does
it clear own target `3A8`. Raw EAX zero is incidental, not a Boolean result.
The native function's original declaration owner is unknown; selection by
this typed primary table establishes this narrow consumer's operation.

The [source](../../../Winx/Code/wxKnutAttackAIAction.h) uses existing storage-only
[`wxAIActionCommandExitHost`](../../../Winx/Analysis/Host/wxAIActionCommandExitHost.h)
and the actual guarded narrow operations on
[`wxAICommandRecordForAnalysis`](../../../Winx/Code/wxAICommandRecordForAnalysis.h).
Its operation list names those concrete data algorithms; no callback supplies
a completed cleanup result, and whole Reset is not substituted for the stores.
The [command contract](../../game/ai/ai-command-reset.md) admits only the explicit
captured profile and raw `703090` tag before each byte write. These APIs are
our raw storage operations, not newly attributed native command methods.

## Supplied state and limits

The constructor binds supplied postconstruction target and all 92 opaque
bytes at `3AC..407`, plus a borrowed host. Inherited owner is a
separate explicit input. Default profile refuses; the admitted profile requires
an empty inherited action graph and stable, live, disjoint records within
each call. Raw tail bytes can contain former encoded container or owned-pointer
data: they remain inert and are not followed, constructed or destroyed.

The typed native action and `wxKnutBossAIBehavior` initial-stage
captures identify the supplied records. They do not prove that game production
binds that behavior to this action. Owner `144`, originally null in the capture,
is explicitly bound to a structural actor field view whose `130` supplies a
borrowed 116-byte command. The actor's native class, extent, RTTI and physical
inheritance are unknown; the portable host neither casts a native character
layout nor defines an invented foreign class. The command's native class
identity and inheritance likewise remain unknown.

Missing owner, host, actor or command, or refused command admission, stops
before the next unadmitted access/store. Every completed byte-store prefix
is retained; target clears only after the full list. This is analytical
admission, not native fault recovery or rollback. Repeated calls and explicit
between-call binding/raw-byte changes are supported. Within-call mutation,
aliasing, concurrency and reentry remain unqualified.

All 92 opaque own bytes, parent duration/clearable words, owner/actor links,
command bytes outside the listed destinations, other commands and target
canaries remain unchanged. Generic factory and Clone throw `logic_error`;
Copy returns analytical false, and Notify, Entry, Init and other unknown hooks
refuse. Borrowed records survive portable teardown. The
[paired driver](../../../Winx/Tests/wxTrollKnutAttackAIActionTests.cpp) checks
complete represented bytes, each fresh chain and retained refusal prefixes.
Native construction/destruction, owning lifetimes, nonempty graphs, PS2,
connected gameplay and whole-class completeness remain open.
