# wxMenuGameFlowState

`wxMenuGameFlowState` is a physical descendant of `wxGameFlowState`, with class
ID `0C73327A`. The native PC and PS2 instances occupy `0x5C` bytes. The portable
class preserves a bounded parent contract: PC construction, destruction,
factory/clone/Copy, and the active hooks corresponding to PC slots `24`, `28`
and `2C`. Its portable layout is separate from the two 32-bit ABI records.

| Native member | Construction and ownership |
| --- | --- |
| `3C` | Null raw allocator allocation; explicitly freed during destruction |
| `40`, `44` | Raw view and signed-comparison mode words, both initially one |
| `48` | Zero word |
| `4C` | Null direct-owned polymorphic object; virtual delete with flag one |
| `50` | Separately allocated `spTaskTimer`, distinct from parent timer `2C` |
| `54` | Second borrowed input-member read, performed after subscribing |
| `58` | Zero byte; padding `59..5B` remains unwritten |

Construction subscribes tag `20`, rereads the input binding, creates the extra
timer, calls its actual Reset operation, reloads `50`, then calls actual Start.
The extra timer remains active. The native virtual slots are `+28` for Reset and
`+20` for Start; confusing these with the named portable timer methods changes
the constructor state. The parent constructor currently models PC only. The
Host profile selects the three descendant hooks; it does not qualify a PS2
parent constructor.

Destruction reads and directly deletes `4C`, then reloads and frees `3C`, then
reloads and directly deletes `50`, unsubscribes tag `20`, and destroys the parent.
Each nonnull field is cleared after its corresponding call. Foreign destructor
or allocator callbacks can change the next field; the next read sees that change.
Fields changed by the final unsubscribe are retained. Overwritten allocations
do not acquire an implicit second release. Storage lives in a required Host
arena; storage-only Take operations return the exact allocation, while source
code itself invokes real virtual destruction. Native direct ownership and
portable arena storage are separate responsibilities.

Hook `24` sends the optional borrowed target a synchronous eight-word packet
`[2764,0,0,0,this,0,0,0]`, disables the current view, clears inherited byte `25`,
disables audio, requests HUD zero, invokes the exit service with one, and resets
a freshly resolved input member `1C`. It does not clear `25` again, so later
callback changes survive. A null packet target skips only its dispatch.

Hook `28` resolves the current view's window, applies it, then resolves the GUI
manager again and requests HUD zero. PC passes flag zero to window lookup and
no argument to apply. PS2 passes no lookup flag and passes zero to apply.

Hook `2C` captures the raw mode, resets input, and applies that captured mode
when its signed 32-bit interpretation is less than ten. It reloads member `1C`
from the captured owner for this call. PC captures mode after the first input
read; PS2 captures it before that read. Negative raw modes take the mode call.
It applies the current view's window using flag one, then freshly resolves
input member `18` and reads byte `18`. A nonzero byte writes shared `45` zero,
requests HUD zero, then disables audio. A zero byte writes shared `45` one,
enables audio, then requests HUD one. Both paths finally clear inherited `25`.
Every later manager, owner, view and shared-word read preserves the native
order across foreign callbacks.

The notification method and Copy are literal native no-ops. Clone constructs a
fresh state, registers the source/clone pair, then invokes inherited Copy;
runtime fields, allocations, borrowed input and timers are not transferred.

Entry `20`, owned collection/retry method `44`, and input/navigation dispatch
`30` explicitly refuse. The collection helper is not the separate entity
broadcast implementation, and null default `4C` does not make it a successful
no-op. Extended native slots `34/38/3C/40` have no portable API in this contract.
Foreign GUI, controller, input and service methods are required dependencies;
their bodies, rendering, global startup and complete native class behavior
remain outside this reconstruction. PS2 fresh post-call segments that reach
return are still segments, not whole-hook execution.
