# `spEntityManager`: runtime entity ownership and dispatch

Статус: имя, общий class/base ID, factory/clone, singleton storage, PS2 exact
allocation `0x20`, PC observed prefix `0x20`, container offsets and the native
add/remove/clear/dispatch operations are confirmed. Original header, method
names and concrete STL typedefs have not been recovered.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x48A15BCB` | `0x48A15BCB` |
| Base | `spBaseObject / 0x415352A1` | `spBaseObject / 0x415352A1` |
| Registration | `0x0075DD28` | `0x004A8940` |
| Initializer | `0x006D27B0` | `0x00482040` |
| Factory | protected `.rld` entry `0x00420190` | `0x0014EE90` |
| Main/support vtables | `0x006DC458` / folded by MSVC | `0x0048DF90 / 0x0048DFB4` |
| Singleton storage | `0x0075CE98` | `0x0049F9AC` |
| Extent | observed prefix through `0x1F` | exact `0x20` allocation |

PS2 factory `0x0014EE90` requests exactly `0x20` bytes. Constructor tail
initializes a container at `+0x14`: count `+0x14 = 0`, while `+0x18/+0x1C`
point at the inline sentinel. PC uses its platform STL representation instead:
`+0x18` is a heap sentinel and `+0x1C` the count; `+0x14` remains allocator or
container state. PC allocation size is not promoted to exact while its factory
remains behind `.rld`, even though all observed accesses end at `+0x1F`.

The extra virtual slots agree semantically across the two builds:

- add (`PC 0x004200D0`, PS2 `0x0014EB70`) appends an object pointer;
- remove (`0x00420070`, `0x0014EA90`) unlinks the matching pointer without
  deleting the object and returns success;
- clear (`0x0041FE40`, `0x0014E9A0`) optionally destroys all pointed objects,
  unlinks every node and resets count;
- dispatch (`0x0041FF90`, `0x0014E670`) walks the list and invokes the first
  post-destructor `spBaseObject` notification slot on every entity;
- list access (`0x0041FD00`, `0x0014E630`) exposes the native container.

Clone (`PC 0x004201F0`, PS2 `0x0014ED90`) constructs a fresh manager, registers
the source/destination pair with clone machinery and calls the inherited
no-payload copy slot. Consequently the runtime entity list is not cloned.
The larger functions `0x0041FEA0` / `0x0014E740` repeatedly issue an event
record of kind `0x1C` and compact a snapshot; `0x0041FD60` / `0x0014E930`
issues kind `0x1E`. Their source-level protocol and event type remain unknown,
so the portable implementation does not invent them.

`Sparkplug/Code/Sparkplug/spEntityManager.h/.cpp` implements an ownership-safe
portable facade using `unique_ptr`, empty-clone behavior and notification
dispatch. This deliberately replaces the native raw-pointer/delete flag with
explicit transfer on removal; it is behavioral reconstruction, not host ABI.

Open: original TU/header/API names, direct PC allocation proof, exact meaning
of `+0x14` on PC, source type of the singleton-support interface, event kinds
`0x1C/0x1E`, mutation semantics when callbacks edit the list, and concrete
derived entity families owned by the manager.
