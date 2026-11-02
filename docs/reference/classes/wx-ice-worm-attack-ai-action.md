# wxIceWormAttackAIAction

`wxIceWormAttackAIAction` has class ID `76850CC0`, PC extent `0x3D0`
(976 bytes) and a physical [`wxAIAction`](wx-ai-action.md) prefix.
Factory `5ADFA0` passes its allocated receiver unchanged to constructor
`5ADC80`. That constructor calls `58FF00` on the same receiver and installs
primary table `7090E8`. Getter `5ADAF0` returns record `767190`.
The [PC layout](../../../Winx/Analysis/PC/wxIceWormAttackAIActionAbi.h)
keeps native placement separate from portable C++ layout.

## Two fresh command chains

Primary cell `2C` selects complete Exit `5ADB90`, a 45-byte body containing
11 instructions and no calls or branches. It reads
`own20 → owner144 → actor130`, clears command byte `1D`, reads that entire
chain again, clears command byte `20`, then clears own target `3A8`.
The second chain follows the first byte store. Raw EAX zero is incidental
and does not establish a Boolean result.

The [source](../../../Winx/Code/wxIceWormAttackAIAction.h) resolves both
chains through the existing storage-only
[`wxAIActionCommandExitHost`](../../../Winx/Analysis/Host/wxAIActionCommandExitHost.h).
It calls the actual shared
[`wxAICommandRecordForAnalysis`](../../../Winx/Code/wxAICommandRecordForAnalysis.h)
operations for `1D` and `20`; it does not call whole Reset or accept a
callback with a completed cleanup result. The shared
[command contract](../../game/ai/ai-command-reset.md) requires the explicit
captured profile and raw `703090` tag before each store.

Native receiver attribution uses the captured typed action and
`wxIceWormAIBehavior` owner. Their former pointer graphs are not made live
by copying their bytes. Owner `144` is explicitly bound to an existing actor
field view and that actor's `130` to a live borrowed 116-byte command record.
The actor view is structural: its native class, extent, RTTI and physical
inheritance are unknown. Portable source does not cast foreign native
layouts or define an invented actor class. The command's native class name,
RTTI and physical inheritance likewise remain unknown.

## Admission and preservation

The constructor binds supplied postconstruction target and 36 opaque bytes
at `3AC..3CF`, with an optional borrowed storage host. The inherited owner
is a separate explicit binding. Default profile refuses Exit. The admitted
profile requires an empty inherited action graph and live, stable, disjoint
owner, actor, command and target records throughout each call.

Missing owner, storage, actor or command, or refused command admission, stops
before the next unadmitted access or store. An already completed `1D` store
is retained if the second chain is refused; target `3A8` is cleared only
after both stores. These are analytical admission results, not a model of
native faults or rollback. Repeated calls and explicit between-call binding
and opaque-byte changes are supported; within-call mutation, aliasing,
concurrency and reentry remain unqualified.

All 36 own opaque bytes, parent duration/clearable words, owner identity,
actor links, command bytes outside `1D/20`, other commands and target
canaries remain unchanged. Source owns none of the borrowed records.
Generic factory and Clone throw `logic_error`, Copy returns analytical false,
and Notify, Entry, Init and other unknown hooks refuse. The
[driver](../../../Winx/Tests/wxIceWormAttackAIActionTests.cpp) checks complete
represented byte snapshots, fresh storage reads, refusal prefixes and
borrowed lifetime preservation. Native construction/destruction, owning
lifetime, nonempty action graphs, PS2, connected gameplay and whole-class
completeness remain open.
