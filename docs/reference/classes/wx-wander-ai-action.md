# wxWanderAIAction

`wxWanderAIAction` (`5E7865EA`) physically and by registration derives from
[`wxAIAction`](wx-ai-action.md). The PC factory `5AB280` allocates `0x410`,
calls constructor `5AB0A0` and the shared AIAction constructor `58FF00`, and
installs primary table `708ED8`. Getter `5AB090` returns record `767070`.
The [native layout](../../../Winx/Analysis/PC/wxWanderAIActionAbi.h) is separate
from the portable C++ object and its captured state.

The [implementation](../../../Winx/Code/wxWanderAIAction.h) provides the whole
own PC Entry at `5AB420`, table slot `28` (portable PS2-offset name `vfunc_30`).
It uses explicit postconstruction state and borrowed owner/entity/plain Node
bindings. It does not reconstruct the native constructor or PathFinder state.

## Entry position capture

Entry first writes zero to own words `3B8` and `3BC`. It then reads the owner
at action `20`, the entity link at owner `24`, the Node at entity `24`, and the
three local-position words at Node `20/24/28`. It stores the position at action
`3A8/3AC/3B0`. X and Y pass through x87 `FLD/FSTP`; Z is copied through integer
registers. The non-stack store order is `3B8, 3BC, 3A8, 3B0, 3AC`. No calls occur
in this body, and the stack argument is unused. The final EAX value has no
declared Boolean meaning.

The admitted portable profile preserves all finite binary32 X/Y values,
including signed zero and subnormals, under the captured x87 environment.
Z is represented as a raw word, preserving integer-copy payloads. All other
own tail words `3A8..40F`, except the five destinations, remain captured opaque
data. The original PathFinder-shaped tail and pointer-shaped words are neither
interpreted nor dereferenced by this Entry reconstruction.

The [host](../../../Winx/Analysis/Host/wxWanderAIActionHost.h) supplies the live
owner/entity/Node storage binding. It does not choose the stores or compute a
position. The exact plain `spNode` type is admitted; derived Node behavior,
concurrent input changes and callbacks during this call remain unqualified.

## Admission and ownership

The default profile refuses before effects. The explicit captured profile
requires an empty inherited current-action/owned-action graph. Missing owner,
entity or Node and nonfinite X/Y produce analytical refusal after the two
initial clears. These refusals do not reproduce a native fault or promise
rollback. The position words remain unchanged when the binding is refused.

The portable constructor copies the supplied `104` own-tail bytes as raw words;
it does not own native pointers encoded in them. Owner, entity and Node stay
live through each synchronous call and remain borrowed after portable action
destruction. Native factory and supporting teardown establish receiver and
borrowed-control evidence separately from portable constructor equivalence.

Generic factory creation requires explicit capture bindings and throws
`logic_error`; Copy returns analytical false and Clone throws. Notify and other
unqualified hooks report refusal through `LastResultForAnalysis`. A void hook
refusal is not a reconstructed native no-op.

The [checks](../../../Winx/Tests/wxWanderAIActionTests.cpp) cover raw position
payloads, complete own-tail preservation, initial-clear ordering, repeated Entry,
graph/profile/type refusals and borrowed lifetime. Native Copy/Clone, full
construction/destruction, PathFinder, other hooks, nonfinite X/Y, arbitrary
graphs, PS2, connected gameplay and whole-class completeness remain open.
