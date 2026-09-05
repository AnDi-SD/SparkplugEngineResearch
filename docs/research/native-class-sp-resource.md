# `spResource`: named resource identity and manager lifetime hook

Статус: identity/base, exact storage-free `0x14` layout, concrete factory,
constructor/destructor, name-copying clone and resource-manager unregister hook
are confirmed on PC and PS2. Original declaration path and manager API name are
not recovered.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x46F043FE / spNamedObject` | same |
| Registration / initializer | `0x007603A0 / 0x006D3A60` | `0x004A9CD0 / 0x00482F90` |
| Factory / constructor | `0x004679C0 /` protected body | `0x0017D050 / 0x0017CF40` |
| Destructor / clone | `0x00467960 / 0x00467A30` | `0x0017CEB0 / 0x0017CF80` |
| Getter / vtable | `0x004679B0 / 0x006E8414` | `0x0017CEA0 / 0x0048EEC0` |
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

Open: original header/TU and exact add/remove method names. The native remove
scans the non-owning cache and erases only the first matching pointer;
unregistered resources therefore perform a harmless unsuccessful probe.
