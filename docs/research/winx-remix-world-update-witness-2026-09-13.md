# PC world-update phase witness for independent Remix input

13 September 2026. This is an adapter hook contract, not a reconstruction change.
The bounded static evidence is [world-update-witness JSON](../../research/winx-remix-world-update-witness-2026-09-13.json).
It records pristine/debug image hashes, five byte ranges, decoded instructions
and two complete bounded vtables. All captured ranges and tables agree between
the two images. No game, GPU, protected native execution or new emulation was run.

The recommended boundary consists of a `45A7D0` manager scope and a completion
observer on the plain `spNode` world-update vtable word `6DC524`. The second hook
identifies the actual root called by the native manager. No extra scene-list
traversal or repeated world update is required.

## Exact hook ABI

| Target | Receiver and arguments | Return/cleanup | Hook qualification |
|---|---|---|---|
| `45A7D0`, SceneManager update | `ECX = complete spSceneManager`; no explicit arguments | Void semantics; plain `ret` at `45A802`. Neither `AL` nor `EAX` is a success result. | Direct entry; no Update slot in primary vtable `6E7154`. Displace **7** bytes `56 57 8B F9 8B 47 18`; resume `45A7D7`. |
| `421420`, Node world update | `ECX = complete spNode`; one stack `u32 inheritedFlags` | `ret 4` at `421630`. Visible final path returns residual `EAX = [node+B0] & ~7`; no boolean-success protocol. | Prefer primary `6DC4F4`, slot 12 / offset `30`, word **`6DC524`**, expected original `421420`. If an entry trampoline were needed: displace **5** bytes `83 EC 3C 53 55`; resume `421425`. |

The manager prefix contains no relative control flow. Five bytes would split the
last `mov`, so a five-byte displaced prefix is invalid. The Node prefix is also
instruction-complete. Its protected branch at `421428` dispatches through
`[13B1510]`; this task does not claim a newly decoded complete protected body.
The slot patch avoids relocating that branch.

For ABI transparency a wrapper may preserve the full original `uint32_t` EAX
through its own post-hook work. This does not turn either method into a boolean
function. The manager does not inspect the root's return register. In particular,
the portable `bool spSceneManager::UpdateWorldForAnalysis` and its guarded failure
policy are not the native ABI.

## What the native manager actually processes

[CP6](native-pc-scene-world.md) and the complete 51-byte `45A7D0` body agree:

- The receiver is the actual `ECX`, not an engine field inferred from the current
  renderer. The engine call at `41CDF3` loads singleton **`[75DB90]`**, creating
  it through `45ADF0` if necessary. The first observer cohort should require the
  receiver to equal the current singleton and its expected manager identity.
- `manager+18` points to the owned doubly linked list sentinel. Each 12-byte
  element is `next+0`, `previous+4`, borrowed `scene+8`. The count at `manager+1C`
  does not bound the native loop.
- At `45A7E3`, the loop writes the actual scene to `manager+20`, loads
  **`scene+14`**, and calls `[root.vtable+30](0)` at `45A7EF`.
- The exact root-call return address is **`45A7F2`**. Only after that call does
  the manager read the current list element's `next` pointer.
- There is no scene `24/25` or Node Enabled `200` test, no null guard, and no
  early-false path. Normal completion, including an empty list, clears
  `manager+20` at `45A7F9` and returns.

There is no native SEH cleanup around the loop. An exception can leave
`manager+20` set. The observer must restore its own TLS state and reject its own
pending witness; it must not clear the game's field or claim native recovery.
Arbitrary reentry or callback list mutation is not made safe by the post-call
`next` read.

An after-manager callback alone proves that the loop returned, but does not prove
which scene/root identities it processed if those identities changed during the
call. Equal bounded pre/post list snapshots cannot exclude transient mutation.

## Minimal exact per-scene witness

CP6 establishes that scene constructor `45EA10` creates a plain `spNode` through
`421E20` as **`scene+14`**, the System Root. This differs from the partition root
reached through **`scene+38 -> partitionSystem+1D4`**.

The adapter can observe this verified native path without changing its inputs:

1. At every manager-update entry, invalidate the preceding eligible witness and
   allocate a new monotonically increasing `updateSerial`. Record the manager,
   render-thread identity, adapter frame, device epoch and owner/topology mutation
   epoch in a bounded TLS batch. A repeated update in the same frame gets a new
   serial. Call the original manager exactly once.
2. In the Node slot wrapper, capture the actual caller return address before
   delegating. Admit a pending root observation only with an active qualified
   manager batch, return address **`45A7F2`**, argument `inheritedFlags == 0`,
   `manager.currentScene20 == scene`, `scene.systemRoot14 == actual ECX`, and the
   exact plain Node primary/installed world-slot identity. Save these identities
   before the original root call. Normal descendant calls returning to
   **`421614`** do not qualify.
3. Call the original root update once. After its normal return, re-read the
   manager/scene/root identities and mutation epoch, then record the pending
   completion token only if they still agree. Preserve the original full EAX;
   neither zero nor a nonzero value affects observer eligibility. Do not update
   native dirty fields, recompute hierarchy state or call any producer again.
4. Publish the batch's bounded per-scene tokens only after normal outer-manager
   return, `manager.currentScene20 == 0`, unchanged singleton/epoch/thread/frame
   identities, and no nested or invalidated batch. A failure rejects pending
   tokens; a later successful update can establish a new serial.
5. Consume a token only outside an active manager batch, when its serial equals
   the latest begun and successfully completed update, and current scene,
   system root, partition root, device/frame and lifecycle epoch still match.
   Root/scene pointers remain borrowed; the token does not extend object lifetime.

A nested manager update should conservatively invalidate both affected observer
batches while preserving the game's original calls. A foreign-thread entry must
atomically invalidate freshness without touching another thread's ordinary TLS
or audit counters. The common epoch is an invalidation mechanism, not permission
to dereference a concurrently retired object. If observer reads fault, reject
observation. If the original faults, propagate its fault after restoring adapter
scope. Use a POD `__try/__finally` invocation helper where needed; do not place
C++ objects requiring destruction in an MSVC SEH function.

Clearing prior freshness at **begin** matters: if update N succeeded, but N+1 in
the same frame fails, retaining N would falsely make the unfinished current world
phase eligible. Merely comparing Present frame numbers does not prevent this.

The exact plain-Node slot intentionally does not cover an unknown derived System
Root override. A derived override that calls base `421420` internally has a
different direct return address; completion of that base call is not completion
of the outer override.

For RenderNode independent inputs, additionally validate its current parent
chain to the witnessed `scene+14` and current native dirty state. Partition
registration alone does not establish world-update coverage. This root witness
does not, by itself, prove that an arbitrary descendant world override traversed
all its children: qualify known traversal semantics for the initial cohort or
retain that case as observer-only evidence. Fresh parent/dirty reads and a final
epoch fence are still required when assembling an input packet.

## Relation to rendering and Present

[CP5](native-pc-engine-frame.md) places `45A7D0` after animation and GUI updates,
but before audio, particles and the engine event queues. The native app executes
the common update **before** its foreground-window graphics decision. It may run
multiple updates while backgrounded without graphics or Present. The verified
CP6 frame probe establishes original app -> animation -> world propagation for
its explicit fixture; it is not a measurement of live update/Present cadence.

Consequently `updateSerial` is independent of the adapter's `frameId`. The
current probe advances `frameId` after Device Present or SwapChain Present,
including failed calls; device/reset invalidation needs its separate epoch.
Recording equal begin/end frame numbers is an additional conservative guard,
not proof that exactly one world update belongs to a Present.

An after-world token means that a native update completed at that phase. Later
events can still mutate PRS, parent links or material/controller inputs before
rendering. It does not establish immutability, producer-free materials or safe
replay of a previously visible packet. Those input guards remain in the
[ordinary independent-input contract](winx-remix-independent-ordinary-input-contract-2026-09-13.md).

## Evidence and remaining verification

Static bytes, x86 cleanup, dispatch identity, ignored return values and the
manager's complete loop are newly checked in the JSON. Scene construction and
original frame behavior reuse CP6/CP5 evidence, without rerunning their probes.
Shared layouts and addresses are in
[`SparkplugAbi.h`](../../Sparkplug/Analysis/PC/SparkplugAbi.h);
[`spSceneManager.cpp`](../../Sparkplug/Code/Sparkplug/spSceneManager.cpp) preserves
the distinct guarded host operation.

No live hook installation, actual update/thread cadence, phase packet count,
exception injection, or custom-descendant coverage is claimed here. The proposed
observer still needs bounded CPU fixtures for normal/empty/reentrant/exception
paths and a live read-only phase comparison before it can authorize independent
scene submission.
