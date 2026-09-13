# PC material → texture: common reference graph (checkpoint 19)

## Confirmed original execution

`41A390` creates the CPU material and `42F690` its serializer. Reading the
material uses `42F670 → 4774D0`. Field 10 invokes common resolver `4678B0`
with expected class `spTexture` (`2F281E13`); an inline runtime texture
`3F3651B6` is constructed by `4AB520` and read by `4B2950`.
The expected-class argument alone is not a native safety check: the common
resolver's previously documented behavior remains unchanged.

After read, clear the non-owning FAT, switch operation to 2, then call
`4672C0` to index recursively: material ID 1, texture ID 2. The original writer
emits field 10 with `UInt32BeginEnd`, invoking the same common writer core.
A second reference is ID 2 / size 0, not a duplicate texture body.

| Native assertions | Result |
| ---: | --- |
| 11 | one canonical texture and exact output |
| 11 | repeated field points to same object |
| 11 | both holders share one texture; refcount 2 |
| 10 | NULL clears fallback; no texture in output |
| 7 | field without a layer is skipped, no texture constructed |

Highest measured read: 40,057 instructions; highest arena position: 58,368
bytes. Every native allocation and texture/surface reference is released at
teardown; device baseline reference remains 1, and no surface remains locked.
Caps stay 100,000 instructions / 2 seconds per call, 64 KiB arena, 32 KiB
allocation request, 30 seconds per child. No original OS/GPU calls forwarded.

## Ownership and a rejected NULL hypothesis

Fields 11/12 differ: their NULL resolver result skips the setter. Also the
earlier PC controller labels were reversed. The verified PC layout is:

| PC material texture offset | Role | Reader/setter |
| --- | --- | --- |
| `34` | fallback `spTexture` | field 10 / `41E870` |
| `38` | `spUVController` (`1C0053D6`) | field 12 / `467D90` |
| `3C..5F` | nine-float UV matrix | field 9 |
| `60` | static UV enable byte | field 9 |
| `64` | `spAnimTexController` (`16FB0E47`) | field 11 / `476680` |
