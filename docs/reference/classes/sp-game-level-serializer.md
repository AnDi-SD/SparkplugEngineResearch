# spGameLevelSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spGameLevelSerializer](../../../Sparkplug/Code/Sparkplug/spGameLevelSerializer.h).

Статус: exact source path, identity/base, exact `0x184` layout, constructor,
platform cleanup difference, blank clone, binary header/record shape and the
main level → template → instance flow are confirmed on PC and PS2. Symmetric
write flow and complete failure rollback remain open.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0x72B27469` | `0x72B27469` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Main read | `0x005FE080` | `0x00158D30` |
| Per-instance read | same surrounding TU | `0x00158970` |
| Exact allocation | `0x184` | `0x184` |

The exact PC path is
`Z:\Sparkplug\Code\Sparkplug\spGameLevelSerializer.cpp`. Both constructors
clear target `spGameLevel*` at `+0x10`, input `spStream*` at `+0x14`, and parser
state at `+0x18`. The remaining `0x168` bytes form one parser-populated instance
record:

| Offset | Size | Role |
| ---: | ---: | --- |
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

Both clone routines construct a fresh serializer, register the clone pair and
reuse root no-payload copy. Target, stream, parser record and in-flight progress
are not copied. The portable class implements only this lifecycle/binding
boundary; it does not advertise an approximate level parser.

Open: original public method names, full `0x48` header, XML root vocabulary,
path normalization and template-load flags, rollback after partial insertion,
trailing level-resource setup, write format, and exact progress callback.
