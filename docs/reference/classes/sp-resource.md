# spResource

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spResource](../../../Sparkplug/Code/Sparkplug/spResource.h).

Статус: identity/base, exact storage-free `0x14` layout, concrete factory,
constructor/destructor, name-copying clone and resource-manager unregister hook
are confirmed on PC and PS2. Original declaration path and manager API name are
not recovered.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x46F043FE / spNamedObject` | same |
| Exact size | `0x14` | `0x14` |

No resource-local member follows `spNamedObject`: the PS2 factory allocates
exactly `0x14`, and both `spMesh` constructors place their next subobject at
`+0x14`. The PC factory body is under `.rld`, but its derived constructor and
all accesses independently impose the same boundary.

Both clone paths construct/register a fresh `spResource` and reuse the
`spNamedObject` copy slot. Consequently the shared name is retained and there
is no resource payload to duplicate.

The PS2 deleting destructor `0x0017CEB0` first installs the resource vtable,
then lazily resolves the global resource manager and calls `0x0017D3A0` with
this resource before invoking the named-object destructor. PC has the same
lifetime relationship around `0x00467960`, although part of its body is
protected. Portable reconstruction now calls the restored `spResourceManager`
hook when an existing manager is alive. It deliberately does not reproduce
PS2's lazy manager creation from a destructor during static teardown.
