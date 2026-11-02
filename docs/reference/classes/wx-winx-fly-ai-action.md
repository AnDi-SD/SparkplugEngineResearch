# wxWinxFlyAIAction

`wxWinxFlyAIAction` has class ID `67AF3520`, registration and physical parent
[`wxAIAction`](wx-ai-action.md), and PC allocation size `0x3C0`. The PC factory
`5BC5A0` calls the common AIAction constructor `58FF00`, installs primary table
`709DD8`, then initializes its own tail. Getter `5BC580` returns record `7678B0`.
The [PC layout](../../../Winx/Analysis/PC/wxWinxFlyAIActionAbi.h) is evidence for
the original allocation, separate from the portable object's C++ layout.

The [implementation](../../../Winx/Code/wxWinxFlyAIAction.h) provides the whole
PC Entry `5BC6A0` under an explicit postconstruction profile with an existing
[`wxCharacterRegistry`](../../../Winx/Code/wxCharacterRegistry.h). Captured
typed state establishes the receiver domain; this profile does not reconstruct
native construction, the global Registry producer or an owning lifetime.

## Entry and the shared Registry algorithm

Entry reads owner `20`, actor `owner144` and command `actor130`. A nonnull
command receives DWORD zero at `04`; a null command skips this store. It then
reads the existing Registry binding and calls actual selector `4E21D0`, returning
to `5BC6D6`. The portable source calls the common Registry implementation
directly. The [host](../../../Winx/Analysis/Host/wxWinxFlyAIActionHost.h) supplies
live borrowed storage and the Registry object, rather than a selected target.

After selector completion, Entry writes zero to own words `3AC`, `3B8`, byte
`3BC`, words `3B0`, `3B4`, then stores the actual selected pointer at `3A8`.
The order is preserved. Padding `3BD..3BF`, other own/base cells, owner and
borrowed target/vector records remain unchanged. The argument is unused, and
EAX is not assigned an invented Boolean meaning.

The shared selector returns an existing nonnull cache immediately. Otherwise
it considers only the first group: empty groups or empty first-group targets
yield null, while a positive first-group target vector supplies and caches its
first pointer, including a null pointer payload. Later groups are ignored.
That behavior belongs to the common Registry source, rather than a separate
WinxFly target-search implementation.

## Captured state and refusal boundaries

The default Fly profile refuses before effects. The admitted profile requires
an empty inherited action graph, stable live owner/actor/nullable-command data
and an existing Registry admitted by its own borrowed-vector profile. Explicit
between-call cache/target data changes are input controls; they do not recover
Registry reset or insertion methods. Inputs remain stable within each call.

Missing owner or actor storage produces analytical refusal. A missing existing
Registry or a refused Registry selection after a command store retains that
completed command prefix and leaves own fields unchanged. These results do not
model the native cold-creation fallback, a fault or rollback.

The portable constructor binds captured raw own bytes and borrowed identities;
it does not own encoded native pointers or recreate PathFinder. Generic factory
and Clone require unsupported native context and throw `logic_error`; Copy
returns analytical false. Notify and other unqualified hooks report refusal.
All borrowed controls and the Registry remain live after portable Fly destruction.

The [checks](../../../Winx/Tests/wxWinxFlyAIActionTests.cpp) cover command-null
handling, real shared Registry selection, complete raw own-tail/command state,
cache repeats and explicit between-call changes, profile/graph/refusal prefixes
and borrowed lifetime. Native constructor/destructor/global-producer and vector
ownership, malformed inputs, cold Registry creation, other hooks, PS2, connected
gameplay and whole-class completeness remain open.
