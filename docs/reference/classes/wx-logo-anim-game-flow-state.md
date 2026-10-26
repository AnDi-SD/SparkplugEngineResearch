# `wxLogoAnimGameFlowState`

`wxLogoAnimGameFlowState` has ClassID `2B43A28A`, native extent `88`, and a
physical `wxGameFlowState` prefix at offset `0` on PC and PS2. Offsets and
native constants below are hexadecimal. Its portable declaration and slot names
are analytical. Parent construction and lifetime use the qualified PC contract;
own PS2 hook behavior is described independently.

| Own field | Constructor behavior |
| --- | --- |
| `3C` | Zero, then a second current singleton member `18` borrow after the parent |
| `40`, `41` | Raw bytes `1`, `0` |
| `44`, `48`, `4C`, `50`, `54` | Zero words |
| `58–7B` | Inline `spTimer`, constructed without starting it |
| `7C`, `80` | Zero words |
| `42–43`, `84–87` | Unwritten; they are not zero defaults |

The concrete producer/type of the reference at `50` remains unknown. The
portable model uses a base-object pointer and an independently stored native
16-bit reference word at `8`; C++ ownership counts do not replace that word.
Cached input `3C` is borrowed and distinct from the parent's earlier cache.

Construction subscribes the state with tag `13`. Destruction unsubscribes,
destroys the inline timer, releases reference `50`, and then destroys the parent.
The reference is read after unsubscribe and inline timer destruction. Release
decrements the unsigned 16-bit count, invokes deleting virtual destruction with
flag `1` only when the result is zero, and does not clear `50` during destruction.
The portable inline timer has an explicit lifetime to retain that ordering.

Entry at slot `20` performs the following sequence:

1. Resolves an independent global service. Its PC entry is a literal no-op;
   the corresponding PS2 method is a required foreign service operation.
2. Resolves the audio interface/controller and passes enabled value `0`.
3. Captures the current input owner, resets its member `1C`, reloads member
   `1C` from that captured owner, and passes mode `0`. This member differs from
   the constructor's cached member `18`.
4. Clears inherited byte `25`, then runs the standalone shared resource
   loading operation described below.
5. Calls the existing shared `spTimer` Reset and then Start on inline `58`.
   These make two distinct required clock samples. The second Start preserves
   the accumulation left by Reset and its callback.
6. Reads application byte `4B`; if it is nonzero, clears four separate global
   words. Otherwise it tests application word `60` and clears them only when
   that word is nonzero. PC retains the captured application owner for this
   second read. PS2 reloads the global owner before reading `60`.

Those four globals are PC `742D48/742D54/742D60/742D6C` and PS2
`4674F4/467500/46750C/467518`. They are represented as four portable raw words;
their wider meaning is not assigned here.

Exit at slot `24` resolves the service and supplies tag `GLOBAL`. The PC method
is a literal no-op; PS2 requires the external service body. Exit then releases
reference `50` using the same unsigned decrement/delete-zero decision and
clears `50` **after** the deleting callback. A callback replacement is therefore
overwritten by that clear. Slot `2C` separately performs captured-owner input
Reset, member `1C` reload, and mode `0`.

Copy is the inherited successful no-op, preserving an existing destination's
own fields, reference, cached input, and both timers. Clone constructs fresh
defaults, borrows current input, registers the clone pair, and applies that Copy.
It does not retain the source's reference `50` or copy its inline timer state.

The operation at PC `597480` / PS2 `28F720` is implemented in
[`wxLogoSharedResourceLoading`](../../../Winx/Code/wxLogoSharedResourceLoading.h).
Its original C++ declaration owner is unknown; no new native member attribution
is made. It runs twice in order, for `loading_empty.stx` and `loading_full.stx`:

- Resolve a real `wxAssetManager` and directly reuse its shared
  `BuildPathForAnalysis` with capacity `100`, category `4`, subcategory `0`, and
  the literal filename. Both load calls receive the same temporary buffer.
- Resolve the current typed `spResourceManager` and request the resource.
  The loader body remains an explicit foreign dependency.
- Read the corresponding shared global **after** Load returns. If old and new
  pointers alias, do nothing. Otherwise release old via unsigned 16-bit
  decrement/delete-zero, retain new via unsigned increment, then store new.

The shared globals are PC `766214/766218` and PS2 `GP-4378/GP-4374`. Slot storage
addresses and live reference-word addresses remain valid during the synchronous
operation; callbacks may change their contents or the service globals. New is
captured across old's deleting callback, and service globals are resolved again
for the second iteration. Counter arithmetic wraps modulo `10000`.

The native caller ignores the path builder's return status. Portable loading
requires an initialized catalog and a path shorter than `100` bytes; failure is
an analytical guard before Load. Uninitialized buffer use, oversized names,
allocation failure, native lazy startup, registry/filesystem implementation,
exception behavior, and dangling results from foreign callbacks are not admitted
by this contract. Safe temporary storage does not establish native buffer-tail
defaults.

The substantial resource transition/FPU body at slot `30`, including its own
helpers, remains unrecovered and the portable boundary refuses it. PC whole
Entry/Exit and standalone loading are distinct from PS2 foreign-call and
fresh post-call segments. A staged PS2 segment reaching the original return
does not establish a whole Entry/Exit/loading operation. PS2 parent startup,
heap/lifetime, clock wrapper, unresolved service bodies, hardware, and whole
class/gameplay completeness remain open.
