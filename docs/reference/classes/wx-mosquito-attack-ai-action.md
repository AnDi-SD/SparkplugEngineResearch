# wxMosquitoAttackAIAction

`wxMosquitoAttackAIAction` has class ID `4FA810CA`, PC extent `3CC`, factory
`5B49A0`, table `7096D0` and getter `5B4900` returning runtime record `767490`.
The factory forwards its allocation unchanged to constructor `5B48A0`. That
constructor calls `58FF00` on the same receiver before storing its own table,
establishing the physical [`wxAIAction`](wx-ai-action.md) prefix independently
from the registered relationship. Portable layout differs from the
[PC layout](../../../Winx/Analysis/PC/wxMosquitoAttackAIActionAbi.h).

The [source facade](../../../Winx/Code/wxMosquitoAttackAIAction.h) admits one
captured PC Exit profile. Its constructor takes a borrowed target3A8 and the
complete32-byte raw tail3AC..3CB. Encoded pointers remain opaque data. This
adapter does not reproduce native construction or ownership.

PC primary slot2C selects the whole66-byte Exit at `5B4920`. It loads owner20,
character144 and command130, then clears command byte1D. It independently
reloads that chain and clears command byte20. A third owner20/character144
chain obtains controller12C and clears its raw word1E0. Finally it clears own
target3A8. Other action, command and controller bytes remain unchanged. The
method has no callee or target dereference and returns RET0; raw EAX is not a
portable Boolean contract.

The [host interface](../../../Winx/Analysis/Host/wxMosquitoAttackAIActionHost.h)
provides structural reads and writable borrowed byte cells. Source performs
the two byte stores itself and invokes the narrow raw-field storage operation
on the real shared [`wxCharacterMoveCtrl`](wx-character-move-ctrl.md) captured
facade. It does not substitute a successful command or movement operation.
The1E0 store is separate from the controller's fifteen-word Reset, and no
standalone native controller method is inferred from the analytical API.

The default profile is Unqualified. The admitted profile requires an empty
inherited action graph, live stable disjoint owner/character/command records
and a typed captured controller. These analytical views do not establish the
native classes of the owner, character or116-byte command record. Bindings do
not change during a call. Explicit repeated Exit calls still execute all four
stores; the profile does not admit other active-hook composition.

Missing later data bindings cause analytical refusals that retain completed
byte-store prefixes. They do not model native null faults or rollback. Generic
factory, Copy, Clone, Notify and other hooks explicitly refuse. Portable action
destruction preserves borrowed records; native construction, owning lifetime,
old encoded pointers, general movement/command semantics, aliasing, concurrent
or mutable bindings, protected methods, PS2, gameplay and whole-class behavior
remain unqualified.

[Tests](../../../Winx/Tests/wxMosquitoAttackAIActionTests.cpp) compare normalized
targets, raw action tail and the controller's represented field20, entity
bytes28..123 and own bytes124..1C17. The full native7192-byte controller guard
is separate evidence and is not a portable object-layout claim.
