# PC native texture data: shared source wrapper and reconstructed reader

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
