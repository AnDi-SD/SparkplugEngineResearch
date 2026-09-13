# spTemplateInstance

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spNode](../../../Sparkplug/Code/Sparkplug/spNode.h), [spTemplateInstance](../../../Sparkplug/Code/Sparkplug/spTemplateInstance.h).

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0x1F6A7DA5` | `0x1F6A7DA5` |
| Base | `spNamedObject / 0x44DE07FD` | `spNamedObject / 0x44DE07FD` |
| Notification entry | `0x00420B40` | `0x00154900` |
| Exact allocation | `0x28` | `0x28` |

Both constructors clear `+0x14`, initialize an empty native list and retain a new `spNode` at `+0x24`. The node is created through PC `0x00421E20` or PS2 `0x001A9160` and is named exactly `Instance Root`.

The list ABI differs while the enclosing size remains equal:

- PC: allocator state `+0x18`, heap sentinel `+0x1C`, count `+0x20`;
- PS2: count `+0x18`, inline sentinel links `+0x1C/+0x20`.

PC performs the corresponding compiler-specific container and intrusive-reference cleanup. The notification entries forward to the common base protocol (PC `0x0040F960`, PS2 `0x00100610`).

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
