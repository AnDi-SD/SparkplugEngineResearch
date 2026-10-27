# wxMenuGameFlowState

`wxMenuGameFlowState` is a physical descendant of `wxGameFlowState`, with class
ID `0C73327A`. The native PC and PS2 instances occupy `0x5C` bytes. The portable
class preserves a bounded parent contract: PC construction, destruction,
factory/clone/Copy, hooks `24`, `28` and `2C`, PC dispatch slot `30` with
its shared navigation operation, and PC Entry `20`/owned initialization `44`. Its portable layout is separate from the two
32-bit ABI records.

| Native member | Construction and ownership |
| --- | --- |
| `3C` | Null owned resource-path byte buffer; explicitly freed during destruction |
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
Host profile selects platform-specific descendant hook paths; it does not
qualify a PS2 parent constructor.

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
manager again and requests HUD zero. Both platforms pass only the view to
window lookup, then pass flag zero to apply. On PC the two pushes precede the
lookup call, but each callee consumes one word: lookup returns with `RET 4`,
and apply reads the remaining flag and also returns with `RET 4`.

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

PC slot `30` updates nonnull extra timer `50`, then reads inherited latch byte
`25`. A nonzero latch and nonzero byte `1B` of the cached input `54` clear the
latch and resolve the current view's window. A nonnull widget at window `134`
supplies a borrowed name buffer. The method invokes foreign flow command
`5954C0` with `(53,0)`, compares the current contents of that same buffer, and
invokes foreign command `595450` with the matching code and a second zero.

| Exact widget name | Code |
| --- | --- |
| `LoadGardenia_01`, `LoadGardenia_02`, `LoadGardenia_03` | `1`, `2`, `3` |
| `LoadGardenia_04` | `50` |
| `LoadDomino_01` through `LoadDomino_05` | `4` through `8` |
| `LoadSwamp_01` through `LoadSwamp_03` | `9` through `11` |

Names are case sensitive and must match through the terminating zero; prefixes
and longer suffixes do not match. A foreign callback may edit the retained name
buffer, replace the flow manager, change view `40`, or overwrite latch `25`.
The next reads preserve those effects. A null widget skips both commands.
When activation is not taken, byte `1B` is read again; zero rearms latch `25`.
The activation path does not rearm it or overwrite a later callback change.

Every completed slot `30` path captures the latest view and directly executes
the shared [PC navigation operation](../../game/flow/game-flow-navigation-remap.md),
including the actual parent timer `2C` update. It then returns true. Parent
input `30` and menu input `54` remain distinct borrowed bindings. Null extra
timer `50` skips only its own update; a paused extra timer preserves its paused
state while navigation still updates the parent timer. Slot `30` refuses the
PS2 profile and retains the shared navigation's finite-angle and cycle guards.
Missing foreign providers are analytical refusals at the point of use, not
native success or native fault behavior.

The PC Menu table also binds shared code at slot `40` to replace member `3C`'s
resource-path byte buffer. Its original declaration owner remains unknown;
the portable method describes the typed Menu consumer. It frees the current
nonnull buffer and clears `3C` after that storage callback. Null input then
finishes without allocating. Otherwise it reads the current NUL-terminated
input length, requests length plus one bytes from the storage provider,
publishes the returned pointer into `3C`, and copies the then-current input
bytes through the first NUL. The native operation consumes one argument and
does not establish a fixed return value.

The source pointer must remain externally alive and separate from the released
buffer. Storage callbacks may change its contents: a free callback changes the
subsequent length, while an allocation callback can shorten or replace the
copied bytes within the requested capacity. Copy stops at the new terminator;
bytes beyond it remain storage-provider data. Callback writes to `3C` are
overwritten by the native clear/publication, with no implicit extra release.
Other callback effects survive. The source publishes null and refuses when
allocation fails; it also preserves completed copy effects and refuses if the
input grows beyond the requested allocation. These are analytical limits,
without a native allocation-failure, overflow or dangling-input qualification.

PC slot `38` is a separate literal true return without state changes. Its code
is shared, with no recovered original declaration owner. Both portable methods
refuse PS2. Slot `34`'s resource/stream/GameLevel loading transaction remains
open. Slot `3C` has an unresolved indirect jump after nonnull object deletion
in the pinned PC image, so its full nonnull path is not implemented here.

The notification method and Copy are literal native no-ops. Clone constructs a
fresh state, registers the source/clone pair, then invokes inherited Copy;
runtime fields, allocations, borrowed input and timers are not transferred.

PC owned slot `44` resolves Core member `18` as the Scene, reads the Scene's
projection manager at `2C` and directly runs its
[initialization operation](sp-projection-manager.md). This field is a manager,
not a task timer. It then rereads Menu member `4C` as the established
`spGameLevel` and invokes the shared
[collection/Notify1C retry operation](../../engine/architecture/game-level-initialization.md).
That operation preserves instance/attached-object order and repeated Entity
pointers; it does not retain inputs or use a supplied whole-helper callback.

After notifications complete, slot `44` resolves Core and Scene again, reads
the SystemRoot Node at Scene `14`, calls its actual world update with zero,
ignores its return value and returns true. Projection callbacks may replace
Menu `4C` before that field is read. Level notifications may replace Core or
Scene before the final root read. Both timers remain untouched by these own
operations. Missing providers, an absent/wrong-type established level, or a
shared helper analytical limit refuse at the reached prefix; native faults
and failure semantics are not inferred from those guards.

PC Entry `20` calls virtual `44` and ignores its boolean result, enables the
current view, then freshly resolves input owner member `18` and reads its byte
`18`. A nonzero byte writes shared flag `45` zero, requests HUD zero, and
disables audio. A zero byte writes shared flag `45` one, enables audio, and
requests HUD one. Manager and shared-field reads occur after the preceding
callbacks. Entry has no trailing clear of inherited latch `25`; UI/audio
mutations of that byte survive. PS2 Entry and owned `44` explicitly refuse.

The existing level, instances, attached entities and root graph must remain
alive during the synchronous operation. Supplying their established graph is
separate from recovering its producer/loader. Nonempty Menu-owned level graph
destruction is also separate from this Entry/initialization contract. The
qualified composition currently uses empty projection lists or already bound
FX helpers; missing-helper acquisition, material/render/device work and full
Scene startup remain open.

Extended native slots `34/3C` have no portable API in this contract.
Foreign GUI, controller, input, flow-command and service methods are required
dependencies; their bodies, rendering, global startup and complete native class behavior
remain outside this reconstruction. PS2 fresh post-call segments that reach
return are still segments, not whole-hook execution.
