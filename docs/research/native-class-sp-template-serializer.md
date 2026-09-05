# `spTemplateSerializer`: template XML/binary boundary

Статус: exact source path, identity/base, exact `0x1F4` layout, constructor and
destructor ownership, target/input binding, blank clone, XML vocabulary and
the main parsed descriptor fields are confirmed on PC and PS2. Complete child
recursion, property-stream emission and write grammar remain evidence-only.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x41577707` | `0x41577707` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration / initializer | `0x00768D70 / 0x006D7910` | `0x004A8B60 / 0x00482250` |
| Factory / constructor | `0x005FC480 / 0x005FC2A0` | `0x001582A0 / 0x00158170` |
| Destructor / clone | `0x005FC220 / 0x005FC4E0` | `0x001580E0 / 0x001581E0` |
| Registration getter / vtable | `0x005FC290 / 0x00711034` | `0x00157500 / 0x0048E140` |
| Header/read entry | `0x005FC310` | `0x00157DA0` |
| Object decode region | starts `0x005FC530` | `0x00157520` |
| Exact allocation | `0x1F4` | `0x1F4` |

The PC binary contains the exact path
`Z:\Sparkplug\Code\Sparkplug\spTemplateSerializer.cpp` and the class string.
PS2 independently repeats the same class ID, base, size and field accesses.

## Layout recovered so far

| Offset | Size | Confirmed role |
|---:|---:|---|
| `+0x10` | `4` | non-owning target `spTemplateObject*` |
| `+0x14` | `4` | non-owning input `spStream*` |
| `+0x18` | `4` | owned parsed-document/tree object |
| `+0x1C` | `4` | `Type` value for the current template object |
| `+0x20` | `0x100` | `AssetPath` |
| `+0x120` | `4` | `ID` |
| `+0x124` | `4` | signed `ParentID`, constructor default `-1` |
| `+0x128` | `0x40` | `ParentName`, constructor-cleared |
| `+0x168` | `0x40` | `Name` |
| `+0x1A8` | `0x0C` | `Position` XYZ |
| `+0x1B4` | `0x24` | rotation matrix derived from `Rotation` data |
| `+0x1D8` | `0x0C` | `Scale` XYZ |
| `+0x1E4` | `4` | owned output/property-stream object |
| `+0x1E8` | `1` | output/status flag, constructor zero |
| `+0x1EC` | `4` | integer output/status field, constructor zero |
| `+0x1F0` | `1` | failure/status flag, constructor zero |

The vocabulary is not guessed from behavior: PC read-only data used by
`0x005FC530` contains the exact strings `Template`, `TemplateObject`, `Type`,
`Name`, `AssetPath`, `ID`, `ParentID`, `ParentName`, `Position`, `Rotation`, and
`Scale`. Position and scale parse three floating-point components. Rotation
parses four components and calls a helper that writes the nine-float matrix at
`+0x1B4`.

The constructor only clears `+0x10/+0x14/+0x18`, sets ParentID to `-1`, clears
ParentName and resets the output/status tail. Most descriptor fields are
parser-owned and remain invalid until a read succeeds. As with
`spTemplateObject`, the portable reconstruction does not manufacture plausible
default asset data.

Destructor ownership is narrow and useful: it destroys the parsed structure
at `+0x18` and the polymorphic output object at `+0x1E4`, then invokes the root
destructor. It does not own target `+0x10` or input stream `+0x14`.

PC `0x005FC4E0` and PS2 `0x001581E0` allocate a fresh serializer, register the
clone pair and reuse `spBaseObject`'s no-payload copy path. Consequently no
target, stream, parsed tree, descriptor or output state is copied. The
portable clone test explicitly guards this boundary.

The remaining functions cover XML tag traversal, child-template recursion,
property-stream creation and a write path. Surviving errors include
`Failed to load child template`; those routines are deliberately not exposed
as a finished parser until their order, rollback and ownership contracts are
closed on both platforms.

Open: exact method names/signatures, parsed-tree concrete type, `Type` enum,
all status-tail meanings, binary descriptor grammar, child recursion and ID
resolution, property-stream output type, and symmetric write format.
