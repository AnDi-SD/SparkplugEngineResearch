# `spTemplateInstance`: named runtime instance root

Статус: identity/base, exact `0x28` allocation, constructor state, root-node
creation, notification forwarding, destruction ownership and blank-runtime clone
are confirmed on PC and PS2. The large relationship-building algorithms remain
evidence-only until `spNode` and the template serializer are reconstructed.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x1F6A7DA5` | `0x1F6A7DA5` |
| Base | `spNamedObject / 0x44DE07FD` | `spNamedObject / 0x44DE07FD` |
| Registration / initializer | `0x00768D10 / 0x006D78E0` | `0x004A8A40 / 0x00482170` |
| Factory / constructor | `0x005FB870 / 0x005FB6D0` | `0x00155C90 /` inline in factory |
| Destructor / clone | `0x005FB100 / 0x005FB8D0` | `0x00155940 / 0x00155B80` |
| Registration getter / vtable | `0x005FB1C0 / 0x00710F80` | `0x001548F0 / 0x0048E0A8` |
| Notification entry | `0x00420B40` | `0x00154900` |
| Exact allocation | `0x28` | `0x28` |

Both constructors clear `+0x14`, initialize an empty native list and retain a
new `spNode` at `+0x24`. The node is created through PC `0x00421E20` or PS2
`0x001A9160` and is named exactly `Instance Root`. Its native class ID is
`0x695C0F65`; the node itself is not yet reconstructed, so the portable class
uses a clearly documented `spNamedObject` dependency seam instead of inventing
an `spNode` layout.

The list ABI differs while the enclosing size remains equal:

- PC: allocator state `+0x18`, heap sentinel `+0x1C`, count `+0x20`;
- PS2: count `+0x18`, inline sentinel links `+0x1C/+0x20`.

PS2 destruction walks the attached list, checks objects against node ID
`0x695C0F65`, releases them, clears the container, and finally releases the
root reference. PC performs the corresponding compiler-specific container and
intrusive-reference cleanup. The notification entries forward to the common
base protocol (PC `0x0040F960`, PS2 `0x00100610`).

Clone is intentionally narrower than a full runtime graph copy. PC `0x005FB8D0`
and PS2 `0x00155B80` construct a fresh empty list and fresh `Instance Root`,
register the source/destination pair with the clone manager, then use the
inherited named-object copy path. Thus the instance's own inherited name is
copied, while attached relationships and root contents are not.

The larger functions in PS2 `0x00154910..0x00155930` and the corresponding PC
region build and query template-instance relationships. Their exact contracts,
key format and ownership transitions cannot yet be named safely. They stay out
of the portable API rather than being approximated.

No exact original header or translation-unit path has yet been recovered for
this class. Its registration is contiguous with `spTemplateManager`, while the
later related class is independently tied to the exact source string
`Z:\Sparkplug\Code\Sparkplug\spTemplateObject.cpp`; contiguity alone is not
treated as proof that all three declarations shared that file.

Open: original declaration path and method names, meaning of `+0x14`, attached
list element type, relationship key format, the full build/query algorithms,
and replacement of the temporary root seam after exact `spNode` reconstruction.
