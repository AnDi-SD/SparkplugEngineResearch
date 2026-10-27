# spBoxProjection: PC center, size and inherited Copy

`spBoxProjection` (`0x32BB2F56`) is a concrete
[Projection](sp-projection.md). PC factory `0x0041A150` allocates `0xB8` bytes.
Its own constructor writes center `9C..A4` to zero and size `A8..B0` to one;
word `B4` is unassigned. The
[portable source](../../../Sparkplug/Code/Sparkplug/spBoxProjection.h) preserves
those values over the actual shared Renderable/Projection hierarchy.

The sphere worker copies the center verbatim and computes
`radius = sqrt(sizeX² + sizeY² + sizeZ²) * 0.5`. Sphere access updates the
cached sphere. The bounds worker computes `center - size*0.5` followed by
`center + size*0.5`; it does not update the sphere. Negative sizes leave the
two bounds unsorted. Signed zeros in the center and size are preserved.

The admitted numerical profile uses finite integer center/size values in
`[-1024,1024]`, including signed zero, with default nearest rounding. Products
and their sum are exact below `2^24`. The source also retains the native X
wide product versus Y/Z float spills in the bounds calculation. The fixture
input setter rejects other values before mutation; it is not a reconstructed
native setter or a native false-return rule. Arbitrary floats, nonfinite data
and other FP modes remain unqualified.

The [Box serializer](sp-box-projection-serializer.md) is a separate native
producer of the two raw geometry vectors. It preserves all 32-bit encodings
without evaluating them; reading those values does not extend the numerical
profile of the sphere/bounds workers.

Copy is the inherited Renderable operation. It preserves the destination's
center, size, cached sphere, flags, opaque records, bound pointer and FX.
Clone creates a new Box, records the actual shared CloneManager pair and calls
that inherited Copy. Consequently, the clone keeps constructor geometry and
own flags even when the source geometry differs. Material/name/common-field
ownership follows the existing shared classes rather than a separate Box
implementation.

The admitted lifecycle domain has null bound pointer `84`, an empty opaque
record container and either no FX or a real PCFX with empty cache. Common
constructor Debug data is explicit, as described by Projection. The original
allocation, container node and FX interface offsets are recorded in the
analytical layout, not reproduced by the portable C++ ABI.

Draw/debug paths, bound-data producers, the unassigned `B4` cache, populated
records, material-backed initialization, native allocation failures, PS2 and
whole-class completeness remain open.
