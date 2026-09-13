# PC TextureData native writer and shared section core

Original DXData writer `42BF40` consumes **CPU spTextureData** with mip vector
at6C, not runtime spDXTexture. Reader42DD10 still creates runtime DXTexture;
input/output object layouts must not be conflated.

## Original sequence

`42E5F0` writes source wrapper; absent source yields `22 00 00`.
Local field6 has platform6 for policy1; policies0/2 select platform7 and additional
cross field0 (predicate429760 on manager18). Cross helper42DD70 writes nested
field5 from independently initialized embedded CPU texture buffer.
Native field1 invokes **42B9E0**, without any pixel conversion.

First native field0:
`u8 flag68; u32 width,height,textureFlags20; u8 field1C; u32 mipWidth,rowStride,rows; pixels`.
Later native field1 records: `u32 mipWidth,rowStride,rows; pixels`.
The16-byte memory record order is width, rows, rowStride, pointer.
All variable fields use UInt32 Begin/End patching; nested terminators are separate.
Original writer serializes supplied records, even a4x4 top-level-only chain,
without generating missing levels.

## Reconstruction

spTextureData now carries analytical native mip records beside its CPU buffer.
The host caller-input helper validates supported packed power-of-two data and
partial chains; it is not a recovered original Init name or mip generator.
Blank cloning preserves the established name-only/default-container behavior.

The DXData writer uses the same spDataBlockSerializer, SourceNone and extracted
cross-field helpers as CPU texture serialization. Manager policy0/1/2 is explicit
caller configuration. Policy0/2 requires a valid CPU buffer as well as native
records before writing. Runtime DXTexture, empty/stale records, unsupported
conversion flags and write failures are rejected rather than invented.
