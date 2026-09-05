# `spIndexBuffer`: CPU index storage and topology accounting

Статус: class identity, direct base, exact `0x28` layout, constructor/release,
four topology-count transforms, 16/32-bit payload selection, stream grammar,
blank RTTI clone and independent deep-copy helper are confirmed on PC and PS2.
Original enumerator and public method names remain open.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x77D5669F / spBaseObject` | same |
| Registration / initializer | `0x0075FEE0 / 0x006D37F0` | `0x004A8C80 / 0x00482310` |
| Factory / constructor | `0x0045F9D0 /` inline | `0x00159A50 / 0x00159910` |
| Destructor / clone | `0x0045FAA0 / 0x0045FA50` | `0x00159890 / 0x00159960` |
| Registration getter / vtable | `0x0045F820 / 0x006E73B8` | `0x001591F0 / 0x0048E1D0` |
| Public init / core init | `0x0045FAD0 / 0x0045F930` | `0x001595C0 / 0x001594C0` |
| Read / write / release | `0x0045FB80 / 0x0045FC90 / 0x0045F830` | `0x00159330 / 0x00159200 / 0x00159740` |
| Independent deep copy | caller-visible counterpart not isolated | `0x00159780` |
| Exact allocation | `0x28` | `0x28` |

No `spIndexBuffer.cpp` path survives in either executable, so the reconstructed
`Sparkplug/Code/Sparkplug` location is inferred. The class spelling is present
in both RTTI tables. PC text additionally preserves the exact nested type name
`spIndexBuffer::eIndexBufferType` in a mesh initialization assertion. No
enumerator spelling was found.

## Layout and state

Both platform constructors write the same fields:

- `+0x10`: initialized byte, initially zero;
- `+0x14`: `eIndexBufferType`, initially numeric value `2`;
- `+0x18`: primitive count;
- `+0x1C`: stored index count;
- `+0x20`: format flags; bit zero selects four-byte rather than two-byte indices;
- `+0x24`: owned index allocation.

PC `0x0045F830` and PS2 `0x00159740` free `+0x24`, clear that pointer and
`+0x10`, but deliberately retain type, counts and flags. The portable release
facade keeps the same observable metadata transition.

## The four count transforms

The public initializer accepts a primitive count. PC `0x0045FAD0` and PS2
`0x001595C0` transform it into the allocation/index count before calling the
core initializer:

| Numeric type | index count from primitive count | inverse in core init |
|---:|---:|---:|
| `1` | `N` | `I` |
| `2` | `3N` | `I / 3` |
| `3` | `N + 2` | `I - 2` |
| `4` | `2N` | `I / 2` |

PS2 exposes the two inverse helper functions cleanly at `0x001596A0` and
`0x001596F0`; PC emits the same switch/multiply-by-`0xAAAAAAAB` arithmetic.
These equations strongly suggest familiar primitive topologies, but assigning
triangle/list/strip/line enumerator names would still be inference. The source
therefore retains analytical `Type1..Type4` names.

The allocation width is exactly `(formatFlags & 1) ? 4 : 2` bytes per index.
Other bits are retained by the native object but no behavior for them has yet
been observed.

## Serialization and copying

Both stream routines use this binary sequence:

1. 32-bit numeric type;
2. 32-bit primitive count;
3. 32-bit format flags;
4. `indexCount` consecutive two- or four-byte unsigned values.

The read path calls the public count-expanding initializer before reading the
payload. Every header/payload operation is checked; native failure paths emit
separate stream-error messages. The portable implementation reproduces the
grammar and adds overflow/range rejection rather than permitting an unsafe
allocation or silent 16-bit truncation.

There are two intentionally distinct duplication contracts:

- RTTI clone (`0x0045FA50`, `0x00159960`) constructs/registers a new object and
  invokes only the root copy slot, leaving the index buffer blank with default
  type `2`;
- PS2 `0x00159780`, used by mesh-side callers, allocates another buffer,
  reproduces type/count/flags and copies every payload byte when initialized.

Tests cover all four transforms, both clone contracts, 16-bit bounds, retained
metadata after release, and a complete 32-bit stream round trip.

Open: original header/TU and method/enumerator spellings, semantic names of the
four topology values, meaning of format bits above bit zero, whether PC has a
direct counterpart of PS2 `0x00159780` or inlines it at all four callers, and
the precise error-code objects emitted by partial stream failures.
