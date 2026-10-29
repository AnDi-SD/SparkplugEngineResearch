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
forwards only under this same profile and owned-level guard. Extended38/40
remain explicit refusals; no dispatch proof for those neighbors is implied.

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
