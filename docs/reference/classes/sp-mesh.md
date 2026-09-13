# spMesh

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMesh](../../../Sparkplug/Code/Sparkplug/spMesh.h).

Статус: identity/base, null factory/clone, exact common `0x50` layout, two
vtables, bounds-valid flag and indexed min/max pass are confirmed on PC and
PS2. The embedded support object and three trailing pointer roles remain open.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x3F077B6C / spResource` | same |
| Bounds pass / larger init | `0x00424220`-range / `0x00424360` | `0x00159AE0 / 0x00159C80` |
| Exact size | `0x50` | `0x50` |

No original `spMesh` source path survives. Registration has no factory. PC
reuses the root null-clone function at primary slot `+0x08`; PS2 emits an
equivalent class-local null return at `0x00159F60`.

The cross-platform layout is:

- storage-free `spResource` through `+0x13`;
- secondary vtable at `+0x14`;
- a reset/rebuilt `0x10`-byte support object at `+0x18`;
- bounds-valid byte at `+0x28`;
- minimum XYZ at `+0x2C`, maximum XYZ at `+0x38`;
- three constructor-zeroed pointers at `+0x44/+0x48/+0x4C`.

PS2 constructor `0x00159EF0` makes the boundary especially clear. PC
destructor and derived constructors impose the same offsets despite its
protected constructor body.

The pass at PS2 `0x00159AE0` initializes minimum components to largest finite
float (`0x7F7FFFFF`) and maxima to its negative, walks every 16-bit index from
`spIndexBuffer +0x24`, resolves position XYZ in the vertex buffer, and folds
component-wise min/max. The larger entry `0x00159C80` resets the support object,
performs the same traversal and finally sets `+0x28`. PC code around
`0x00424220..0x00424351` performs the same comparisons and stores.

Open: original header/TU and init method name/signature; identity of the
secondary interface and `+0x18` support type; roles/ownership of
`+0x44/+0x48/+0x4C`; exact PC constructor; whether the 32-bit index flag is
unsupported here or handled by another platform-specific path; vertex format
and stride contracts. These are inputs to `spMeshData`, `spRenderMesh` and the
platform vertex-buffer classes.
