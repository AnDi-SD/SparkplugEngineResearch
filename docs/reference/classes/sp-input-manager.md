# spInputManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spInputManager](../../../Sparkplug/Code/Sparkplug/spInputManager.h).

Статус: common identity/base, null factory/clone, singleton, three-subobject
shape on PS2 and platform device-interface boundary are confirmed. Original
TU/header and source-level interface names are absent; `Code/Sparkplug` is an
inferred placement.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0x55A1304D` | `0x55A1304D` |
| Base | `spCrossPlatform / 0x20A72504` | `spCrossPlatform / 0x20A72504` |
| Initializer | `0x006D5B20` | `0x004829E0` |
| Singleton | `0x00755288` | `0x0049F8B8` |

PS2 constructor `0x0016D8F0` calls `spCrossPlatform`, publishes the singleton,
installs a third vptr at `+0x18` and clears byte `+0x1C`. Common destructor
`0x0016D850`, registration getter `0x0016D840`, null clone `0x0016D9C0` and
support thunk `0x0016D9D0` close the lifetime chain. The common registration
has no factory on either platform.
