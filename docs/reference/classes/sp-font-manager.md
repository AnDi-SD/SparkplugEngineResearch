# spFontManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spFontManager](../../../Sparkplug/Code/Sparkplug/spFontManager.h).

## Историческая запись до уточнения11 сентября

Статус: identity/base, registration, singleton, exact platform layouts,
container lifetime, append/name lookup, two default-font references and common
initialization are confirmed on PC/PS2. Original common TU/header and public
method names are absent; `Code/Sparkplug` is an inferred placement.

## Identity and layouts

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID | `0x1640375E` | `0x1640375E` |
| Base | `spCrossPlatform / 0x20A72504` | `spCrossPlatform / 0x20A72504` |
| Initializer | `0x006D2780` | `0x00482960` |
| Singleton | `0x00755270` | `0x0049F8BC` |
| Size | `0x3C` | `0x38` |

The four-byte difference is compiler/container ABI, not derived storage. PS2
constructor `0x00168AC0` proves a pointer-array container at `+0x18`, count at
`+0x1C`, values initialized through `+0x2C`, and intrusive default-font
pointers at `+0x30/+0x34`. PC consumers place the corresponding defaults at
`+0x34/+0x38`; the protected constructor is behind the `.rld` entry
`0x0041FB00`, while the concrete leaf factory independently allocates `0x3C`.

Both common registrations have a null factory. Common clone is null
(`0x004A1BF0` inherited on PC, `0x00168BA0` on PS2); concrete platform leaves
provide their own factory and empty-runtime-state clone.

## Proven behavior

PS2 `0x001685C0` appends a font pointer and increments its 16-bit intrusive
reference count. There is no duplicate check. `0x001688C0` iterates the array,
reads the inherited shared name entry at font `+0x10`, and returns the first
matching object. Destructor `0x00168970` releases every entry, clears the
count, destroys the container and releases both default references.

Initialization `0x00168620` / PC `0x0041E8B0` obtains a renderer-owned font,
builds/configures a companion object and makes the resulting references the
active defaults. The exact `spFont`/render-resource types and original API
spellings are not yet recoverable, so portable methods carry `ForAnalysis` and
accept the proven `spNamedObject` boundary. Actual GPU resources are not
fabricated.

## Reconstruction and open boundary

`Sparkplug/Code/Sparkplug/spFontManager.h/.cpp` implements singleton lifetime,
append semantics, first exact-name lookup, independent defaults and the common
initialization state. PC/PS2 ABI records preserve their distinct offsets.
Tests exercise duplicate insertion, lookup, defaults, clone reset, RTTI and
both layouts; the isolated Windows build passes 2/2.

Still unknown: common source/header path, original container and method names,
the exact string comparator contract, meanings of PC `+0x18..+0x33` and PS2
`+0x24/+0x28/+0x2C`, and the concrete renderer/font classes created during
initialization.
