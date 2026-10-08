# spTemplateSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTemplateSerializer](../../../Sparkplug/Code/Sparkplug/spTemplateSerializer.h).

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0x41577707` | `0x41577707` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Header/read entry | `0x005FC310` | `0x00157DA0` |
| Object decode region | starts `0x005FC530` | `0x00157520` |
| Exact allocation | `0x1F4` | `0x1F4` |

The PC binary contains the exact path
`Z:\Sparkplug\Code\Sparkplug\spTemplateSerializer.cpp` and the class string.
PS2 independently repeats the same class ID, base, size and field accesses.

## Layout recovered so far

| Offset | Size | Confirmed role |
| ---: | ---: | --- |
| `+0x10` | `4` | non-owning target [`spTemplate`](sp-template.md)* |
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

The bound target is the whole `spTemplate`. PC entry `0x005FD580` and PS2
entry `0x00157DA0` store the passed template at `+0x10` and the input stream
at `+0x14`; decoded `spTemplateObject` descriptors are appended to that
template's list by separate calls. Neither the serializer's descriptor scratch
fields nor its parsed output change the type of this target pointer.

The fixed binary header is [specified separately](../../formats/template-binary-header.md).
PC helper `0x005FC310` reads one `0x4C`-byte block into zeroed storage, validates
magic `0xDAB33F00`, writes header word `+0x48` to `template +0x20`, then applies
the name at header `+0x04` through the inherited named-object setter. Its full
32-bit return value is the descriptor count at header `+0x44`. A valid zero
count still changes the target word and name before the outer reader reports
failure. A failed raw read leaves the target untouched; an invalid magic also
leaves it untouched and emits the original diagnostic. The reader trusts the
stream's boolean and does not check an actual byte count: a successful short
read consumes the remaining zero-filled header fields.

`ReadBinaryHeaderForAnalysis` exposes this header operation with a raw count
and the real template binding. The target pointer is read again after the
foreign stream call, preserving a change made by that callback. Missing host
bindings and a name without a null terminator inside its 64-byte field give
an unknown result; this bounds unsafe native inputs without claiming native
rollback. The optional diagnostic callback is explicitly a host forwarding
boundary. Parsing and constructing the following descriptors remain open.

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
all status-tail meanings, binary descriptor grammar after the fixed header, child recursion and ID
resolution, property-stream output type, and symmetric write format.
