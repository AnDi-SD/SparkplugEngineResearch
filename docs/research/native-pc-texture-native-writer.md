# PC TextureData native writer and shared section core

Checkpoint 18, 2026-09-06. Continuing PC SMO/SAN goal.
Original executable SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

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

The bounded fixture supplies small vector/pixel storage to an original CPUData
factory, invokes actual scalar setters/writer/destructor. It does not claim native
vector creation or conversion. Actual destruction releases supplied records and
pixels. Policy0/2 initializes the separate CPU buffer through original475F30.

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

After [CP108](native-pc-texture-missing-mips.md), the DX reader generates missing
raw power-of-two levels. Compressed missing-mip conversion remains open.
Correctly writing the original representation does not establish complete
reader/rendering support. NativeData writer's unchecked
individual writes/error cleanup remain separate research from safer host checks.

## Validation

Six exact original/source cases: raw1, raw2, DXT1-4, raw policy2, raw policy0,
raw4 top-level-only. Original max15121 instructions/1744 arena bytes.
Five complete source outputs additionally load through the reconstructed DX
reader with identical mip bytes; partial output failed at that historical boundary
(CP108 subsequently adds its raw missing-level reader support).
This source roundtrip is not substituted for original full loading.

`pc-texture-native-writer`: **9/9** children, **36 original assertions**, six new
exact comparisons plus three exact shared-CPU regressions. Fresh texture suite
**926/926**, **32/32 CTests (18.90 s)**. No limits changed, cap retries, GPU/OS
forwarding, asset/application edits, publication or PS2 credit.
Still required: complete native write failures, embedded/external sources,
remaining containers/conversion, material/controller links and whole FFPS Save.
