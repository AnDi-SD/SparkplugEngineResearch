# PC Material → ColorController → frame gate

## Canonical field6 reference, index and output

Actual MaterialData serializer42F690/interface42F670 invokes common Material
reader4774D0. Field6 uses **4678B0** then **423A50→423650** on non-NULL.
Already loaded ID7 is supplied as an explicitly prebound object in actual FAT,
not a success callback. Directory validation and all lookup/read/index/write
functions execute original instructions. Cold RTTI/factory41A580/clone41AF70
are denied before entry.

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
