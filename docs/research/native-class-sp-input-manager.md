# `spInputManager`: common input lifetime boundary

Статус: common identity/base, null factory/clone, singleton, three-subobject
shape on PS2 and platform device-interface boundary are confirmed. Original
TU/header and source-level interface names are absent; `Code/Sparkplug` is an
inferred placement.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID | `0x55A1304D` | `0x55A1304D` |
| Base | `spCrossPlatform / 0x20A72504` | `spCrossPlatform / 0x20A72504` |
| Registration | `0x00764EF0` | `0x004A94F0` |
| Initializer | `0x006D5B20` | `0x004829E0` |
| Primary/support vtables | `0x006F3064 / 0x006F3060` | `0x0048E740 / 0x0048E764` |
| Device-interface vtable | protected/unresolved common table | `0x0048E770` |
| Singleton | `0x00755288` | `0x0049F8B8` |

PS2 constructor `0x0016D8F0` calls `spCrossPlatform`, publishes the singleton,
installs a third vptr at `+0x18` and clears byte `+0x1C`. Common destructor
`0x0016D850`, registration getter `0x0016D840`, null clone `0x0016D9C0` and
support thunk `0x0016D9D0` close the lifetime chain. The common registration
has no factory on either platform.

The portable reconstruction preserves the singleton and initialization state,
but exposes only an analytical controller-count seam. It does not merge the
incompatible DirectInput and PS2 pad virtual interfaces. Still open: common
source paths, the exact PC common prefix after `+0x18`, original interface
type/method names, event/state formats and frame-poll semantics.
