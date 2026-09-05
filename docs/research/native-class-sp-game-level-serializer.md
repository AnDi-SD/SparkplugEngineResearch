# `spGameLevelSerializer`: level-to-template-instance loader

Статус: exact source path, identity/base, exact `0x184` layout, constructor,
platform cleanup difference, blank clone, binary header/record shape and the
main level → template → instance flow are confirmed on PC and PS2. Symmetric
write flow and complete failure rollback remain open.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x72B27469` | `0x72B27469` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration / initializer | `0x00768E30 / 0x006D7970` | `0x004A8C20 / 0x004822D0` |
| Factory / constructor | `0x005FDF80 /` inline | `0x00159180 / 0x00159060` |
| Destructor / clone | `0x005FDC90 / 0x005FDFF0` | `0x00159000 / 0x001590A0` |
| Registration getter / vtable | `0x005FDC80 / 0x00711308` | `0x00158960 / 0x0048E1A0` |
| Main read | `0x005FE080` | `0x00158D30` |
| Per-instance read | same surrounding TU | `0x00158970` |
| Exact allocation | `0x184` | `0x184` |

The exact PC path is
`Z:\Sparkplug\Code\Sparkplug\spGameLevelSerializer.cpp`. Both constructors
clear target `spGameLevel*` at `+0x10`, input `spStream*` at `+0x14`, and parser
state at `+0x18`. The remaining `0x168` bytes form one parser-populated instance
record:

| Offset | Size | Role |
|---:|---:|---|
| `+0x1C` | `0x40` | instance name |
| `+0x5C` | `0x100` | template asset path |
| `+0x15C` | `0x0C` | position XYZ |
| `+0x168` | `0x10` | rotation quaternion XYZW |
| `+0x178` | `0x0C` | scale XYZ |

That shape is cross-checked two ways. PC XML parsing writes these exact offsets
from `Position`, `Rotation`, and `Scale` attributes. PS2 binary read requests an
exact `0x168` temporary record, treats its first `0x40` as the name and next
`0x100` as asset path, then reads transforms at `+0x140/+0x14C/+0x15C` relative
to the record start.

The binary header is `0x48` bytes and begins with magic `0x351E46AE`. Its
instance count controls a loop that creates `spTemplateInstance` via the real
factory, reads each record, resolves the asset path through `spTemplateManager`,
and loads/registers the template when absent. It then:

1. stores the resolved template in the instance;
2. copies the instance name;
3. applies position, quaternion-derived rotation and scale to its root node;
4. appends the completed instance to `spGameLevel`;
5. destroys the temporary instance on failure before returning false.

This is direct evidence for the list element role in `spGameLevel` and for the
template manager's place in the runtime load path.

The PC destructor releases the parser tree at `+0x18`; PS2's destructor only
invokes the root destructor because its read path treats that field as
transient. This platform difference is kept in evidence rather than flattened
into a false common ownership rule.

Both clone routines construct a fresh serializer, register the clone pair and
reuse root no-payload copy. Target, stream, parser record and in-flight progress
are not copied. The portable class implements only this lifecycle/binding
boundary; it does not advertise an approximate level parser.

Open: original public method names, full `0x48` header, XML root vocabulary,
path normalization and template-load flags, rollback after partial insertion,
trailing level-resource setup, write format, and exact progress callback.
