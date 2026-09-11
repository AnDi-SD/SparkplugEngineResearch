# PC alpha gate, original sphere metric and shared ordering

Block 10, 11 September 2026. Viewer and the shared WPF backend now classify
transparent draws with the original material/pass/object gate and order them
through common PC metric/comparator code and the explicit versioned CRT policy.
The previous C# distance sort ignored priority and used geometry AABB centers.

## Original behavior reused

The PC pre-routing predicate at `423FD0` requires a non-null material, nonzero
first-pass blend and the renderable AlphaSort flag. Renderer flush/sort flags
are separate. Its existing implementation was extracted from `spSkin` into
`spRenderable::RequiresPCAlphaQueueForAnalysis`, and Skin calls that same method.
The previous portable missing-pass guard is preserved; it is not an original
null guarantee. See [PC renderer protocol](native-pc-renderer-protocol.md) and
[whole Skin enqueue](native-pc-skin-alpha-queue.md).

`spv_graph_alpha_info` reads this predicate, the real renderable sphere getter,
unsigned priority and exact ParticleSystem test. `SmoLoadedResources` stores
the result with each actual loaded Model/Skin. Scene preparation keeps a
separate alpha support matrix: Skin vertex rendering and queue support world
are not interchangeable. Static containers retain their stored world matrix.

`AlphaOrdering.h` passes those values into existing `BuildAlphaKey` and
`CompareAlpha` (`454C30/454800`). Comparison puts ordinary objects before exact
ParticleSystem, orders ordinary objects by descending unsigned priority, then
by descending squared distance. Particle priorities are ignored. Equal and
unordered keys still return +1; there is no invented stable tie comparison.
The sorter is the [shared named CRT policy](tool-shared-sort-policy-2026-09-11.md),
`msvcr71_7_10_7031_4`, rather than another C# or `std::sort` implementation.

The C ABI input/output rows are 88/16 bytes; alpha resource metadata is 28 bytes.
Invalid buffers, flags, count and non-finite input components are refused before
output publication. Priority addition keeps unsigned wrap. Five finite metric
fixtures reproduce both the original comparator/CRT output permutation and
the original distance bits from the already pinned block3 capture. This is
reuse of original evidence, not five newly executed EXE discoveries.

## Host frame policy and integration

`SmoAlphaOrdering` reuses managed transport buffers. The renderer submits one
unit per actual file/renderable/container/member-slot identity, retaining the
order of any geometry pieces within that unit. Hidden placements are omitted.
Repeated slots of one resource remain distinct. Editor world changes update
the support matrix; Viewer animation uses the current actual container world
from its borrowed scene runtime, independently of the Skin palette.

Camera presentation reflection/turntable is placed in the view input. Host
priority bias is zero. `DepthOnlyAlphaMetric` defaults to false and explicitly
selects the original camera byte231 branch when requested; it is not inferred
from WPF OrthographicCamera or serialized game Is2D.

This API is a modern frame batch, not the original owning/borrowing queue or
full visibility scheduler. Its defensive limit is 65,536 items, deliberately
separate from the original queue's 2048-entry capacity, overflow/drop behavior,
flush callbacks and dynamic appends. Those existing original routines remain
available and unchanged. Host-created transient geometry has no current game
sphere and uses its declared preview AABB center through the same native metric;
that path is identified as non-original in the order diagnostic. No original
bounds are fabricated for it. Unsupported sorting reports `ALPHA_ORDER_UNAVAILABLE`.

Existing host opacity overrides still control preview placement transparency.
Loaded first-pass blend is stable in the currently supported material-controller
path; editing a document rebuilds the shared snapshot. Cross-file preview order
does not claim a complete game's Scene/general/shadow/alpha phase schedule.

## Verification

Evidence lives in `local-data/results/tools-core-cycle-20260911-1900/alpha-order/`;
`research/tools-core-alpha-ordering-2026-09-11.json` seals selected sources,
binaries and reports. Native DLL SHA256:
`9347406CB540862C43A7DFA617E0294FF258B4B5307B3FDE06EA7F33B80A15EF`.

- Affected native SkinRender/Msvcr71Sort suites: 2/2 passed, 3.04 s. No
  warnings/errors in this affected native build. Original queue flags,
  borrowing/flush behavior and CRT ties remain covered by those checks.
- `research/check_alpha_order.py`: 89 C ABI checks, 0.0275 s inside its bounded
  child. Five pinned original finite cases include the nine-equal ordering
  `[9,5,3,4,2,6,7,8,1]`; depth-only, priority wrap, exact particle placement,
  canaries and actual Icy Skin/material metadata also pass. The older raw
  NaN/negative-key fixture remains in native CRT tests, outside finite sphere
  input construction here.
- GPU renderer integration: 20 checks. Three declared metadata fixtures use
  one actual mesh upload and distinct occurrence slots. Priority beats distance;
  original-sphere input beats the geometry centroid; the explicit depth branch,
  hidden slots, editor support update and clear behavior are checked.
- Real Alfea02: 1008 placements; 160 raw AlphaSort flags but only **20** actual
  material-gated queue units, with priority bands 4/3/1/0. All use real sphere
  metadata. 8248 pixels covered at 192×192; warm live frames 4.104/4.629/4.800 ms,
  peak 261,165,056 bytes. No claim of a stable FPS speedup follows from this
  small target; the reduction in incorrectly queued units is measured directly.
- Real Icy: 12 placements, **6** queued units from 12 raw flags, 7752 pixels,
  no render errors. Existing material pixel tests: 31 checks. Actual Viewer
  load/SAN/slider plus GPU frames: 43 poses, 13,167 checks, 5.208 s.
- Actual LVLcreator window with `Media/Menus/igmenu_opt_pc.smo`: 207 placements,
  833 existing command/binding/picking checks pass after the shared changes.
  All relevant managed projects build without warnings/errors; whitespace
  checks report no errors.

An initial LVL invocation accidentally selected the distinct `Menus/menu.smo`.
That document exposes a real pre-existing graph gap: object ID26 has class
`spTextNode/52E86EFE`, currently unregistered. Its failure report is preserved;
it was not relabelled as an alpha regression or hidden by a substitute class.
The exact earlier control asset then passed. `spTextNode` is the next priority
because one missing type currently blocks loading the whole menu document.
PS2 alpha runtime and full application-core completion are not claimed.
