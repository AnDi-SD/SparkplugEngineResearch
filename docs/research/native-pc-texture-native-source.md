# PC native texture data: shared source wrapper and reconstructed reader

Status on8 September: [CP115–116](native-pc-texture-cross-upload.md) restore
common raw formats0..4 upload, normalized resize and mip generation.
[CP118](native-pc-texture-external-source.md) restores external source field4
through an explicit owned stream factory, including the original path rule.
Sections below preserve checkpoint16 evidence; its then-open boundaries have
the subsequent updates linked at the end.

Checkpoint 16, 2026-09-06. Continuing research, **not complete SMO/SAN support**.
Primary executable SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Original behavior and compiled reconstruction

`42EA50` source field 3 calls the same selected serializer's payload reader,
using the existing target object. There is no new object header or FFPS file.
Two complete original recursive calls (raw 2x2 and DXT1 4x4, full mip chains)
confirm this, including actual destination pixels and final base texture state.
Field 2 resets the handled flag; unknown source fields are skipped. External
field 4 remains unsupported in the reconstruction until its resolver is traced.

Both CPU and DX data readers now use one shared source-wrapper handler in
`spTextureDataSerializer`, including bounded recursion and restored depth on exit.
The DX adapter implements native platform field 6, nested field 1 and raw mip
records. Field 0 cross conversion is still unavailable on DX; it is not replaced
with successful empty output. The actual registry wire key is `78EA082B`, not
the DX serializer's separate virtual identifier `0B1C67BB`.

The nested first-mip prefix is `u8 nativeFlag, u32 width, height, flags, u8 field1C`;
each mip is `u32 width, rowStride, rows` followed by `rowStride * rows` bytes.
Native flags 0/1/2/3 select raw ARGB/DXT1/DXT3/DXT5. Original `4ABBA0` copies rows
to surfaces using the real COM pitch and does not normalize dimensions.
The portable CPU shadow accepts only supported positive power-of-two dimensions,
matching tight row extents and a complete provided mip chain. Missing levels,
conversion, palette and external sources remain explicit unsupported boundaries.
Those host bounds are safety policies, not claims about native malformed inputs.

Importantly, native-data attachment writes base fields 18/1C/20/24/28/2C but
leaves DX fields 44/48 untouched. Runtime attachment has a different state contract.
The source stores analytical surface format separately from native runtime format;
it cannot invent a valid writer format for fresh native-data input. A runtime
writer also rejects stale/mismatched runtime versus surface format after reinit.
The latter is a host safety guard; full original multi-COM reinit is still open.

## Evidence and validation

`research/compare_pc_texture_native_mip.py` compares actual original copied pixels,
input bytes and base state with the compiled reader. It removes only the declared
four padding bytes per fixture row, never fabricates original COM event traces.
Eleven exact cases: raw/DXT1/DXT3/DXT5 single levels; raw 2/4, DXT1 4, DXT3 2,
DXT5 4 full chains; raw 2 and DXT1 4 embedded source sections.

`pc-texture-native-source`: **12/12** children, **117 native mip assertions plus
5 palette factory assertions**, eleven exact compiled comparisons.
Source texture suite: **719/719**. Fresh build and **32/32 CTests (23.03 s)** pass.
Largest new embedded reader: 16437 instructions, 54592 arena bytes.
All original temporary texture-data allocations and COM fixture references are
released. Limits remain 100k instructions/2 s per native call, 30 s per child,
64 KiB arena and 32 KiB allocation request. No GPU, game process, cap retry,
asset edits, publication or PS2 credit.

## Palette dependency reconnaissance

Original factory `4B2C80` returns **414 bytes**, primary vtable `6F0A6C`;
record getter `4B2C30` returns `763DE0`. Class `spPalette` has ID `591C0B9F`,
direct base `415352A1`, registration initializer `6D52E0`.
Field 10 starts at `FFFFFFFF`; bytes 14..413 (1024 palette bytes) are left
constructor-uninitialized. Factory/getter/destructor all completed and freed
their native allocations. This does not establish renderer palette ownership.
Static clone `4B2CF0` invokes source copy slot `40ECE0` (no-op); dynamic clone,
setter `4B93C0`, renderer slots 48/4C, read/write and destruction with a live
palette need separate evidence. No source palette implementation is claimed yet.

## Still required

[CP108](native-pc-texture-missing-mips.md) subsequently reconstructs missing raw
power-of-two levels through actual61039A/60FDB4, including exact float codec
rounding and wrapped edges. [CP113](native-pc-texture-compressed-mips.md) adds
compressed DXT1/3/5 missing levels with filter4 and no dithering. Arbitrary
conversion remains open; the original checkpoint results below are historical.

Common raw conversion0..4 is now covered by CP115–116 and external-source
orchestration by CP118. Arbitrary palettes/filters, package-backed external
streams and remaining errors still require separate evidence.

Later [checkpoint 17](native-pc-palette-lifetime.md) closes the palette-codec,
construction/copy and observed ownership/failure subset described as pending above.
Full renderer integration is still open.

Real `icebat.smo` embedded texture is only statically framed so far, not loaded
natively: its 32x32 base level likely requires generation of missing mip levels.
Internal conversion `60FDB4/61039A`, native-data writer, external source paths,
palette, material/controller references, reinit/clone/errors, lossless unknown
fields, whole FFPS Save and resource-to-live-PC-render remain open. This extends
[checkpoint 15](native-pc-texture-runtime-mips.md), not a completion claim.
