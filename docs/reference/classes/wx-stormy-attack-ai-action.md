# wxStormyAttackAIAction

`wxStormyAttackAIAction` has class ID `32957C4C`, registration and physical
parent [`wxAIAction`](wx-ai-action.md), and PC allocation size `0x470`. Its PC
factory is `5C86B0`, primary table `70A440`, and getter `5C6450` returns record
`767A90`. The [PC layout](../../../Winx/Analysis/PC/wxStormyAttackAIActionAbi.h)
describes the original allocation separately from the portable C++ object.

The [implementation](../../../Winx/Code/wxStormyAttackAIAction.h) provides whole
PC Entry `5C56F0` and separate Exit `5B79D0`. Entry uses an explicit postconstruction captured-state profile with
an existing [`wxCharacterRegistry`](wx-character-registry.md). Captured typed
state establishes this receiver domain. The profile leaves native construction,
PathFinder, the global Registry producer and owning lifetime open.

## Existing Registry Entry

Entry reads the existing Registry at native global `765AD8` and calls selector
`4E21D0` at `5C570B`, returning to `5C5710`. It does not read owner or command
data. The portable action reads a borrowed
[storage binding](../../../Winx/Analysis/Host/wxCharacterRegistryBinding.h)
afresh each call and calls the common Registry implementation directly.
The binding supplies a Registry object; it does not supply a selected target.

After selector completion, Entry stores the actual selected pointer at `3A8`.
It then writes zero to words `3B0`, `3B8`, byte `3AC`, words `3C4`, `3B4`,
`3C8`, `3D4`, and byte `3F4`, followed by word `3C0 = 1`, byte `3D0 = 1`
and word `3D8 = 2`. This order is retained in source. Other cells, all padding,
parent state and borrowed records remain unchanged. The argument is unused;
the void Entry is not assigned a Boolean return contract.

The shared selector returns a nonnull cache immediately. Otherwise it considers
only the first group. Empty groups or empty first-group targets yield null;
a positive first-group target vector supplies and caches its first pointer,
including a null payload. Later groups are ignored. This behavior belongs to
the common Registry implementation, without an action-specific target search.

## Captured state and refusal boundaries

The portable constructor binds a borrowed target, complete raw `3AC..46F`
bytes and captured owner identity. It does not dereference or own pointers
encoded in those raw bytes. The default profile refuses before effects. The
admitted profile requires an empty inherited action graph and stable live
Registry views within each synchronous call. Binding/cache/target replacement
between calls is an input control, not recovery of native reset or insertion.

A missing Registry or a refused shared selection produces analytical refusal
before any own-field write. A completed null selection is admitted and still
performs all twelve own stores. These boundaries do not model the native
cold-creation fallback, malformed-vector faults or rollback.

Generic factory and Clone throw `logic_error` because their native context is
unqualified; Copy returns analytical false. Notify and other unqualified hooks
report refusal. Portable action destruction leaves the binding, Registry and
borrowed records alive.

The [checks](../../../Winx/Tests/wxStormyAttackAIActionTests.cpp) cover real
shared selection, complete raw own state, parent preservation, repeated calls,
explicit input changes, binding rereads and refusal/lifetime boundaries.
Native construction/destruction/global producer and ownership, other hooks,
PS2 semantics, connected gameplay and whole-class completeness remain open.

## Exit through the common command record

Exit reads `own20 → owner144 → actor130`, captures the existing command and
calls its actual `703090` table cell `04`, selecting Reset `5992E0`. On return
to `5B79E7`, it clears own target `3A8`. The common Reset's 38 command stores
precede this last store; raw `3AC..46F` and parent state remain unchanged.
The argument/result does not receive a Boolean meaning.

The portable action uses a separate default-unbound
[Exit storage host](../../../Winx/Analysis/Host/wxStormyAttackAIActionExitHost.h)
to resolve borrowed actor and command records. It calls the actual shared
[`wxAICommandRecordForAnalysis`](../../../Winx/Code/wxAICommandRecordForAnalysis.h)
algorithm directly. The host supplies no Reset or Exit-success callback.
This analytical data facade has no native class name, RTTI ID or inheritance
claim. Its complete raw `116` bytes come from the observed constructor output;
the portable constructor does not recover native construction.

The common record defaults to refusal. Its explicit PC profile also checks
that captured bytes `0..3` encode table `703090` before any Reset store.
This is analytical admission of the original virtual-call domain; the raw
word is never dereferenced as a C++ vptr. Native Reset itself does not read
the table word. All other untouched bytes, including byte `18` and padding,
remain captured data. The [command contract](../../game/ai/ai-command-reset.md)
describes the destinations and separate historical PS2 evidence.

Exit requires an independently admitted empty-graph profile and stable live
owner/actor/command records. Missing storage or a refused common record leaves
the old own target in place. Entry admission does not admit Exit automatically.
Borrowed command records outlive the portable action. Unknown construction,
other hooks, aliasing, concurrent input changes, native faults, owning cleanup
and native Entry-to-Exit composition remain outside these method profiles.
