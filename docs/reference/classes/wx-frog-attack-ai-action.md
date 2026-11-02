# wxFrogAttackAIAction

`wxFrogAttackAIAction` has class ID `4CFF056C`, PC extent `0x3F0` (1008 bytes)
and a physical [`wxAIAction`](wx-ai-action.md) prefix. Factory `5B3EF0`
passes its allocated receiver unchanged to constructor `5B3D40`. That
constructor calls `58FF00` on the same receiver and installs primary table
`709668`. Getter `5B3DD0` returns record `767430`. The
[PC layout](../../../Winx/Analysis/PC/wxFrogAttackAIActionAbi.h) records these
native facts separately from portable C++ object placement.

## Cached-command Exit

Primary cell `2C` selects complete Exit `5B3ED0`, a 27-byte body with no
calls. It reads cached command pointer `3E8`, clears command byte `1D`,
reloads cached pointer `3E8`, clears command byte `20`, then clears own
target `3A8`. The two cached reads and three stores retain that order.
The method does not clear the whole command or resolve an owner chain.
Its raw EAX zero is not a Boolean success contract.

The [source](../../../Winx/Code/wxFrogAttackAIAction.h) calls narrow `1D`
and `20` storage operations on the shared
[`wxAICommandRecordForAnalysis`](../../../Winx/Code/wxAICommandRecordForAnalysis.h).
These are analytical data APIs, not newly attributed native methods.
The supplied command is the existing 116-byte captured record with raw
primary tag `703090`; its native class name, RTTI identity and physical
inheritance remain unknown. Default profile and raw table-tag admission
follow the [command contract](../../game/ai/ai-command-reset.md).

Raw own ranges `3AC..3E7` (60 bytes) and `3EC..3EF` (4 bytes), parent
duration/clearable words, cached command identity, all other command bytes
and other borrowed records remain unchanged. The native own range
`3AC..3EF` contains 68 bytes including a four-byte pointer at `3E8`;
portable source represents that pointer as a borrowed identity beside
64 raw bytes. This is not a portable 32-bit native ABI claim.

## Supplied state and admission

The constructor accepts explicit postconstruction target, opaque own bytes
and a live cached-command binding. The historical initial cache word is
`CCCCCCCC`, so that initial capture alone does not supply a usable command.
Entry `5B3E40` statically reads `own20 → owner144 → actor130`, stores that
pointer in own `3E8` at `5B3E52`, then sets command `1D`. A later call to
`515350` remains outside the qualified domain. This establishes the cache's
producer role; it does not qualify whole Entry, Init, cache production,
native construction or an owning lifetime. The explicit live cache is a
method input rather than invented constructor behavior.

Exit defaults to `Unqualified` and requires the explicit captured PC profile,
an empty inherited action graph, and live, stable, disjoint borrowed records
throughout each call. Missing cache or refused command admission returns an
analytical refusal before the corresponding store; earlier completed effects
remain. These results describe neither native faults nor rollback. Repeated
calls and explicit between-call cache/target/raw-byte changes are supported;
within-call mutation, aliasing, concurrency and reentry are unqualified.

Generic factory and Clone throw `logic_error`; Copy returns analytical false.
Notify, Entry, Init and all other unknown hooks refuse. Opaque encoded
PathFinder and other captured pointers are not followed or owned. Portable
teardown preserves the borrowed command and targets. Native allocation,
construction/destruction, nonempty graphs, PS2, connected gameplay and
whole-class completeness remain open. The
[driver](../../../Winx/Tests/wxFrogAttackAIActionTests.cpp) keeps complete
command/raw-byte snapshots and exercises the admission and borrowing limits.
