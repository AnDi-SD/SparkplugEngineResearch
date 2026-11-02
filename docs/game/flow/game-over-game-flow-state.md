# wxGameOverGameFlowState

PC `wxGameOverGameFlowState` physically derives from MenuGameFlowState and
registers beneath it with class ID `0x331018ED`. Its native extent is96bytes:
the92byte Menu prefix followed by a borrowed input pointer at5C. The portable
class uses the shared Menu/GameFlow/TaskTimer constructors and lifetime; its
host size and pointer representation differ from the original PC ABI.

After parent construction it sets view40 to32 and obtains a fresh input borrow.
That observation can differ from the parent's earlier30/54 input observations.
The class never releases this borrowed input. The portable constructor is
explicitly host-bound; no implicit RTTI factory is registered.

Own Tick uses raw field25 as an armed flag. When armed, it captures5C once and
reads input1B, then2A only if1B was zero. An asserted value clears25 and requests
the existing foreign flow operation595450 with arguments38hex and0, then
returns true. A callback's state changes survive because no trailing store
follows the request. The flow-controller body and transition are not modeled.

When no request occurs, Tick obtains5C again and reads1B, then conditionally2A
from that captured pointer. Both zero values rearm25 to1. The pointer remains
captured within each pair, including when a data observer changes the member
between reads. Original PC completion guarantees AL1 andret4; portable bool
does not represent the other EAX bits or the native calling convention.

The own host defaults to an unqualified Tick profile. Its admitted profile
requires synchronous live borrowed input records and an existing foreign flow
provider. The extra2A observation reuses the established Menu1B/flow boundaries;
it does not recover wxUserInput device polling or atomic sampling. Missing
bindings cause analytical refusals. A missing flow discovered after25clear
retains that completed effect; this is not native lazy creation or rollback.

Entry has a separate default-Unqualified PC profile. Its admitted domain owns
an exact final empty GameLevel with null name and no instances. The caller
attests existing stable Core/Scene bindings to an empty PCProjectionManager
and a plain detached Node with the captured nine-zero local orientation and
dirty transform. This does not qualify general matrix startup. Entry executes
the actual shared Menu Entry and its measured44 target, which runs the shared
Projection, GameLevel initialization and Node world-update bodies. The44 API
forwards only under this same profile and owned-level guard. Extended38 and40
need their independent profiles described below. Entry does not admit either
neighboring hook.

After the parent returns, Entry freshly resolves the global input owner and
its member18 byte18. A nonzero byte stores shared45=0, applies HUD0, then
foreign input-device0. A zero byte stores shared45=1, applies the input-device1,
then HUD1. Each global binding is observed in that conditional order. Existing
Menu APIs with legacy Audio names refer here to the confirmed foreign DXInput
embedded view/device protocol; neither input polling nor those device bodies
are reconstructed by this operation.

The tail captures a fresh owner, resets its member1C, reloads1C from that same
captured owner and calls SetMode with literal0. It does not read mode44, even
when a callback changes that word. It then resolves the global owner afresh,
stores its structural byte20=1 and clears own25=0. These final stores overwrite
earlier callback writes in order. Entry never consumes cached5C, including a
supplied null decoy. Missing later foreign bindings cause analytical refusals
that preserve completed prefixes, without a native fault/rollback claim.

The original protected legacy runtime remains opaque. Mapped-image observation
does not recover its shared wrapper/lock semantics or imply no additional
module effects. Native frame/CPU/layout/allocation evidence stays separate
from portable typed state and logical foreign observations.

Exit is inherited directly from the shared Menu body through primary slot24;
it has its own default-Unqualified PC profile. The admitted borrowed records
remain alive while observed, and callbacks may replace the global bindings.
Own4C must be null or an exact final empty/null-name GameLevel before effects.
That check is analytical ownership admission: original Exit does not read4C.

The body builds an inline directed frame with code2764, this sender and all
other logical words zero, including the full tail word. A null target skips
only its callback. It resolves GUI freshly, captures current view40 and disables
that view, then clears25. It resolves the foreign DXInput view/device and sets0,
resolves GUI freshly again for HUD0, invokes the existing foreign exit service
with argument1, then resolves a fresh owner/member1C and calls Reset once.
There are no trailing stores; late callback changes to25 survive. Exit does
not consume5C or readmode44, callSetMode, perform another Reset, broadcast a
key11 notification or invoke an extended Menu slot. The unknown exit-service
body/owning class and GUI/input bodies remain foreign boundaries. Raw native
eightDWORD/RET4 evidence is separate from portable pointer-sized logical state.

Clone and the other unqualified active hooks explicitly refuse in this bounded
subclass. These refusals define source scope, not native failure/no-op behavior.
Explicitly qualified Menu base calls remain separate analytical base APIs.
The common literal Copy retains its existing no-transfer semantics. PS2,
original headers, global lifetime, gameplay and whole-class behavior remain open.

Primary slot28 also directly inherits the Menu body. Its separate PC profile
defaults to Unqualified and admits live borrowed GUI/window bindings with null
owned buffer3C and level4C. These ownership checks define the analytical domain;
the original slot28 does not inspect those members.

The body resolves GUI freshly, captures view40 and finds its window. Native
lookup5593A0 consumes only the view argument; literal0 remains on the caller's
stack for the following window55D1C0 apply. The returned window stays captured
even when lookup callbacks change view40 or the GUI global. The body then
resolves GUI freshly for HUD0 and finishes through that foreign tail call.
There is no own25 clear or other native heap store in this operation, so
callback changes to25, mode44 and cached5C survive. Neither input record nor
either real parent timer is consumed. Native stack/tail-return evidence is
separate from the portable logical calls. Foreign GUI bodies, cold creation,
owned nonnull graphs/buffers, repeated/composed calls, PS2 and gameplay remain
unqualified. Missing later bindings preserve completed prefixes as analytical
refusals, without a native fault or rollback claim.

Primary slot2C independently selects the shared Menu body. Its own PC profile
defaults to Unqualified and requires live borrowed input-owner/record/hook,
GUI/window, DXInput view/device and shared-byte bindings. Owned3C/4C remain
null in this bounded domain; the pre-call ownership guard is analytical.

PC first captures the input owner and its member1C, then captures raw mode44
before invoking Reset. It compares that captured word as signed32 against10.
Values below10, including high-bit words, cause SetMode with the unchanged
captured raw value. SetMode obtains a fresh member1C from the same captured
owner, even if Reset replaced the global owner or changed mode44. The native
member-load observation precedes the mode capture on PC.

The body freshly resolves GUI and captures view40 for lookup, then applies
literal1 to the returned window. Lookup and apply each consume one PC stack
argument. Afterwards it resolves the global input owner afresh and observes
member18 byte18. A nonzero captured byte writes shared45=0, applies HUD0,
then resolves the foreign DXInput view/device for0. A zero byte writes
shared45=1, resolves the foreign device for1, then resolves GUI for HUD1.
The shared-byte binding is observed after the input byte, and the conditional
GUI/DX bindings are fresh in that order. Changing the input byte after its
observation does not change the captured branch decision.

Both branches clear own25 after the foreign callbacks, overwriting their25
changes. Callback changes to view40 and mode44 survive. No cached5C read or
extra Reset occurs. Source executes the existing common Menu algorithm;
foreign input, GUI and DXInput methods remain explicit boundaries. Missing
later bindings preserve completed prefixes as analytical refusals. Arbitrary
graphs, cold creation, repeats/composition, PS2 and gameplay remain open.

Extended slot38 has a separate PC profile that defaults to Unqualified. The
GameOver table selects an eight-byte wrapper, which calls the actual shared
three-byte Menu38 literal body and then returns true itself. The portable
implementation retains that common-body call before its own true result.
The original shared declaration owner remains unknown.

The admitted domain has null owned3C/4C and the two live real parent timers;
cached5C may be a null or alternate borrowed decoy. This ownership admission
is analytical. The selected native wrapper/callee do not read or write the
receiver or other heap state, allocate, free, poll input or call GUI methods.
All receiver fields, timers and borrowed input bytes remain unchanged.
Native completion guarantees AL1 and RET0. Other EAX bits retain the supplied
native CPU input and are not represented by portable bool. Separate actual
owned teardown remains distinct from this literal operation. Other profiles,
nonnull ownership, cold startup, repeats/composition, PS2 and gameplay remain
independently unqualified.

Extended slot40 directly selects the shared Menu path-buffer body. Its separate
PC profile defaults to Unqualified. The admitted sequence starts with null3C
and level4C remains null. Every later3C buffer belongs to this object and was
created by an earlier admitted40 call in the same sequence. The two parent
timers and borrowed input records stay alive. Supplied paths are stable live
NUL-terminated byte arrays, disjoint from owned buffers, with at most64 supplied
bytes. The storage callbacks succeed without changing path or receiver state.

The body first captures old3C. A nonnull buffer is freed before3C is cleared.
It then captures the path argument. A null path finishes without allocation;
a nonnull path is scanned through its first NUL and requests length plus one
bytes. The returned storage pointer is published in3C before bytes are copied,
including the terminator. Empty strings allocate one byte; bytes after the
first NUL are not consumed. Native completion is RET4, with no portable EAX
result claim. Source forwards the actual common Menu algorithm and keeps
allocation/free callbacks as storage boundaries.

Explicit replacement and clear sequences free old buffers once. A final live
buffer is released by the actual Menu portion of owned teardown, separately
from the state and two timers. Other receiver fields, timers and borrowed path
and input bytes remain unchanged by slot40. Aliased or dangling paths, callback
mutation/reentry, unsafe length growth, allocation failure, general ownership,
path loading, other hook composition, PS2 and gameplay remain unqualified.
Supporting protected teardown observations do not qualify global effects or
legacy runtime infrastructure.
