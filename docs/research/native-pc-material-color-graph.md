# PC Material → ColorController → frame gate — CP26

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Continuation of [CP25](native-pc-material-color.md); no protected constructor,
whole native startup, in-game/GPU or PS2 evidence is added.

## Canonical field6 reference, index and output

Actual MaterialData serializer42F690/interface42F670 invokes common Material
reader4774D0. Field6 uses **4678B0** then **423A50→423650** on non-NULL.
Already loaded ID7 is supplied as an explicitly prebound object in actual FAT,
not a success callback. Directory validation and all lookup/read/index/write
functions execute original instructions. Cold RTTI/factory41A580/clone41AF70
are denied before entry.

Repeated field6 reuses the same pointer and does not add another retained
edge to the same holder. NULL field6 **preserves** the current controller.
Two materials each retain one edge; the controller's borrowed backlink targets
the last bound material. Updating it changes only that material. Rebinding
restores the previous material's saved colors as established in CP25.

Index4672C0 descends into the color controller; writer emits one material ID
and one controller ID, then nested ColorFunc/scalar payloads through the actual
shared reference writer. The outer material field6 reserves UInt32 length;
the controller payload independently reserves its own UInt32 field0.

Source `spMaterialSerializer` now reads canonical references, binds shared
owners, indexes this edge and writes it with the **existing common core**.
The controller's RTTI factory is still NULL and tested as such: an inline
new controller cannot be invented by the loader. Reading a supplied prebound
object and writing a known declared object are explicitly separate capabilities.
Unknown target types/unowned references fail host validation. Native expected
class parameter alone is not treated as equivalent to host safety validation.

Native tests give declared controller a single explicit external intrusive
pin; material edges are genuinely retained/released above it. Before teardown
actual423A50(NULL) releases each edge; the pin prevents an unproven destructor
on analytical backing. This is not native constructor/lifetime closure.

## DXMaterial secondary slot0 / 4A9530

`ECX=complete+14`, argument is a force byte. The global at **75DB68 is the
renderer**, NOT EngineCore (755274). It reads renderer `+40` stamp and compares
material `+70` (secondary+5C). Equal stamp and force=false skips the body.
Otherwise it stores the new stamp even without an attached controller.

If controller74 exists, compare its accumulated20 and applied1C clocks. Only
unequal clocks invoke original virtual20/4373E0. Force bypasses the frame-stamp
check, **not** the equal-clock check. The test starts stamp0 and accumulates
.25+.5 without updating; force consumes .75, the next stamp consumes .25,
then equal-clock force and next-stamp calls do not reevaluate.

Shared-controller test: first material can trigger a controller bound to the
second; its update changes the second material. The second material's own
subsequent call sees equal clocks and skips evaluation. Random outputs match
the shared MT19937 consumption order, not merely the final alpha.

Static native EndScene4BB9D0 increments secondary renderer+28 = complete+40
after device slotA8. That links the stamp to the renderer scene/frame counter;
its original C++ member spelling is not asserted. Native begin/end full device
protocol is a separate next boundary, not credited from the static xref alone.

Host DXMaterial `UpdateColorForFrameForAnalysis` accepts the renderer stamp
from its caller. No fake full renderer/engine singleton is needed. It keeps
native ordering on finite valid input; host bool is a guard, not a native ABI
return value. Shader/pass application and actual visible draw are still open.

## Verification and runner improvement

**9/9 exact captures**, **91 native assertions** (57 frame +34 reference),
source **213/213**, final build42/42, CTest **37/37**,24.06 seconds
(prior full run42.00s). Native max19,057 frame /20,500 graph instructions;
max graph arena6,688 bytes. Actual tracked allocations freed, no GPU calls.
Source compile initially used a nonexistent FAT include; corrected to the
existing `spResourceFATSerializer.h`, with a local shadow warning also removed.
The preliminary `engine` variable label was corrected to renderer without
changing native addresses or behavior.

`research/native_workbench.py run` now persists a uniquely owned generated
JSON in `local-data/results/bounded-native-runs/` after each child and at
termination: pass/failure/deadline, expected/completed child counts, exit codes,
script hashes, timings and work-item hash. It does not change evidence scores
or automatically repeat failures. Bounds remain30s child/100k+2s native call.
13/13 runner unit tests cover success, deadline-before-start and timeout report.
Example first record:
`20260906T232658334989Z-pc-material-color-graph.json` (passed9/9).
These are execution summaries, not complete transitive binary provenance.

Evidence: `probe_pc_material_color_frame.py`, `probe_pc_material_color_links.py`,
`compare_pc_material_color_graph.py`, common material and DXMaterial source,
`Sparkplug/Tests/spMaterialColorTests.cpp`. No class100 or global gate passed.
