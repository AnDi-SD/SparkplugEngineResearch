# wxGolemAttackAIAction

`wxGolemAttackAIAction` has class ID `4A405186`, PC extent `0x3C0` and physical
[`wxAIAction`](wx-ai-action.md) prefix. Factory `5B7A90` inlines derived
initialization: it forwards the allocated receiver unchanged in ECX to
`58FF00` at `5B7ACE`, then installs primary table `709980`. This is not a
separately attributed Golem constructor entry. Getter `5B79B0` returns record
`767610`. The [PC layout](../../../Winx/Analysis/PC/wxGolemAttackAIActionAbi.h)
is separate from portable C++ placement.

The [implementation](../../../Winx/Code/wxGolemAttackAIAction.h) provides the
complete Exit selected by its primary cell `2C`: common body `5B79D0`.
The original declaration owner of that shared body remains unknown; the
Golem table establishes this class as an actual consumer.

## Captured command Exit

Exit reads `own20 → owner144 → actor130`, captures the command, then executes
its actual `703090` table cell `04` selecting Reset `5992E0`. On return to
`5B79E7`, it clears own target `3A8`. The 38 native command stores precede
that last store. Raw own bytes `3AC..3BF`, including padding, and represented
parent state remain unchanged. The void Exit has no Boolean result contract.

Source calls the existing
[`wxAICommandRecordForAnalysis`](../../../Winx/Code/wxAICommandRecordForAnalysis.h)
directly. A small [shared structural host](../../../Winx/Analysis/Host/wxAIActionCommandExitHost.h)
resolves borrowed actor and actual command records for Golem/Yeti. It supplies
no Reset or Exit-success callback. The common record's native class name,
RTTI ID and inheritance remain unknown. Its default profile and captured PC
table-tag admission are described in the [command contract](../../game/ai/ai-command-reset.md).

## Admission and open behavior

The constructor binds supplied postconstruction target/raw20B state and owner
identity. It does not reconstruct native initialization, PathFinder or the
encoded pointers' lifetime. The default Exit profile refuses before effects.
The explicit PC profile requires an empty inherited action graph and live,
stable borrowed records. Missing owner/actor/command or a refused common
record leaves own target and raw tail unchanged. These are analytical
admission results, not native faults or rollback.

Generic factory and Clone throw `logic_error`; Copy returns analytical false.
Notify, active attack hooks and all other unqualified hooks refuse. Entry
requires its separate binding and profile described below.
Portable teardown leaves borrowed commands and targets alive. Native
construction/destruction/owning graphs, aliasing, concurrent changes, PS2,
connected gameplay and whole-class completeness remain open.

The [dual driver](../../../Winx/Tests/wxCommandExitAIActionTests.cpp) keeps the
Golem and Yeti replay domains separate, including their different raw tails,
real shared Reset, repeated calls and explicit between-call input changes.

## Entry with a positive Registry target and no Entity

PC primary cell `28` selects complete Entry `5B7B90`, ending in RET4 at
`5B7C2D`. It reads existing global `765AD8` and calls the real shared
[`wxCharacterRegistry`](wx-character-registry.md) selector `4E21D0`. The
qualified cache or first-group path supplies a borrowed target; source calls
the same shared algorithm through the existing Registry binding. There is
no callback returning a prepared target or operation result.

The original publishes the selected target to own `3A8`, captures owner `20`,
then writes word `3AC = 0`, word `3B4 = 481C4000`, byte `3BC = 0`, and word
`3B0 = 0`, in that order. `481C4000` is the bit pattern of `160000.0f`;
the field's wider role remains unknown. The other bytes of raw own range
`3AC..3BF`, including adjacent padding, stay unchanged.

Only after these five own stores does Entry read the captured owner's field
`24`. A null value skips virtual Notify `04`; the subsequent target check
skips owner slot `50` when the selected target is nonnull. This domain reaches
the actual whole-method return. The argument is unused, and the raw return
register has no portable Boolean contract. Repeated calls execute the entire
sequence and use the current shared Registry state again.

The typed PC owner used for this domain is a captured `484`-byte
`wxGolemAIBehavior` with primary `6FFA90`, getter `566080`, record `753EE8`
and a null field `24`. This identifies the tested borrowed owner data; it
does not add a portable definition of that class or recover its owning map
and action graph. The [Entry binding](../../../Winx/Analysis/Host/wxGolemAttackAIActionEntryBindings.h)
resolves field `24` as stored data through an explicit owner identity.
It provides no Notify or action-selection method. Old encoded owner pointers
remain opaque and unowned.

Entry admission is independent of Exit and defaults to unbound/unqualified.
The explicit profile requires an empty inherited action graph and live,
stable, disjoint borrowed Registry, owner storage and target records. Missing
or unadmitted Registry storage refuses before class own stores. After a
completed shared selection, source preserves the five-store prefix when
owner storage is missing, Entity `24` is nonnull, or the target is null.
Nonnull Entity refuses before Notify; this refusal precedes the null-target
selector refusal, as in the original control flow. These are analytical
boundaries and do not model native faults, successful foreign calls or
rollback. A completed null Registry result remains valid for the shared
selector but does not qualify the rest of Golem Entry.

`EnterForAnalysis` and primary Entry dispatch report through
`LastEntryResultForAnalysis`; the existing Exit result/profile retain their
separate contract. Tests compare the full raw own range, parent words,
borrowed identities, both Registry caches and owner fields, while preserving
existing Golem/Yeti Exit and Yeti phase drivers. Full native `960`-byte action,
`484`-byte owner and `44`-byte Registry guards remain separate from portable
object layout. No command or Timer is read by this Entry. Cold Registry
creation, nonnull-Entity Notify, null-target owner selection, wider Golem
methods and native Entry/Exit lifecycle composition remain open.
