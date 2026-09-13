# PC texture: runtime flat codec, native mip copy и настоящий registry key

## Разные wire, runtime и serializer identifiers

Actual startup entries `6D1850/6D1880`, `6D18B0/6D18E0`, `6D1910/6D1940`
исполнены на явно пустом manager, затем original4224F0 выбрал reader/writer.
Все шесть registers **один wire ID `78EA082B` (TextureData)**:

| platform mask | operation masks | virtual slot28 value |
| ---: | ---: | ---: |
| 6 | 2 и1 | 0B1C67BB |
| 8 | 2 и1 | 24767C83 |
| 1 | 2 и1 | 78EA082B |

## Runtime DXTextureSerializer: плоский поток

Actual read `4B2950`, write `4B27C0`, factory `4B24C0`, secondary this+10.
Это **не** DataBlock payload DXTextureData:

```text
u32 width, height, runtimeFormat
u8 palettePresent
if(palettePresent == 1): byte palette[1024]
u32 mipCount
packed rows for each level, without field headers or row sizes
```

Runtime formats0..7 map `4AADC0` → DXT1,DXT3,DXT5,15,16,29,17,1A.
Packed row size: compressed max(1,width>>2)*8/16; uncompressed
width*4/4/1/2/2. Compressed rows=max(1,height>>2); otherwiseheight.
Reader `4B26C0` gets desc/locks each surface, uses `4B8560` for packed row
size, reads rows and advances destination by actual COM Pitch. Writer
`4B25A0`/`4AB190` reads corresponding packed rows and **omits pitch padding**.

Reader creates COM texture via4B2680 (levels0,usage0,pool1), then attaches
via4ABAC0, AddRef and releases temporary owner. Sets width28,height2C,
runtimeFormat44,size48,init24,field31false; **does not initialize18/1C/flags20**.
Raw size sums actualPitch*height; compressed sums packed rows. Both retained
texture and all temporary surfaces balance. Synthetic COM storage implements
only declared tiny full chains; original instructions do all row I/O/copy.

## Native DXData mip path completes without conversion

Actual `42C640` (source/local)→`42C3B0` creates temporary TextureData41A2D0,
reads field0 prefix `[byte68,u32width,height,flags20,byte1C]` plus first mip.
Further field1 values append mip records. Wire mip=`width,rowStride,rows,bytes`,
memory record16=`width,rows,rowStride,pointer`. Temporary vector+6C is selected
by flag68 and fully destroyed after upload.

Target DX primary slot24 is `4ABBA0`: flag68 nonzero selects native path;
flags20=0/1/2/3 maps15/DXT1/DXT3/DXT5. CreateTexture uses exact dimensions,
**not** the cross-reader normalization. It copies `rowStride` bytes per row
with original `rep movsd/movsb`, advances by physical destinationPitch,
and balances each surface LockRect(flags10)/Unlock/Release.
`4AB030` generates only missing levels; with full supplied chains no
`60FDB4/61039A` conversion body is entered or replaced.

Native path sets18=GetLevelCount,1C,flags20,width28,height2C,init24.
**It leaves runtime format44 and size48 unchanged**: fresh object still has
allocator poison at44 and zero48. These are not valid inferred defaults.
Consequently runtime flat writer must not be assumed valid automatically
after this distinct nativeData route; its raw-format writer uses44.

## Source reconstruction and host boundaries

`Code/SparkplugDX/spDXTexture.*` and `spDXTextureSerializer.*` use original
class IDs/base names and the shared serializer core. Paths/API spellings are
explicitly inferred/analytical. Runtime header is generic467550; Data/DXData
42DD10 now returns correct **DXTexture CPU shadow**, not unavailable/CPUData.
Native acquisition of device is separately demonstrated in bounded tests;
the portable constructor does not pretend to have acquired a live COM object.

Runtime codec stores packed mip bytes. An explicit optional context pitch
callback models tested padded surface allocation; default tight CPU storage
is **not a promise of actual device pitch**. Total16MiB, positive16-bit
power-of-two dimensions, exact full mip count/extent and pitch≥packedRow
are safe host guards. Reads validate before state replacement; originals
publish/create earlier and have incomplete rollback. Palette==1 fails explicitly.
Other palette marker bytes behave as absent, matching native equality test.
New formatInitialized flag prevents host zero from masquerading as native44.
Unknown source/DXData payload is still rejected; nativeData source adapter is next.

## Real-resource lead and unclosed dependencies

`60FDB4/61039A` internal library bodies remain unclosed. The EXE does contain
explicit D3DX9 shader assembler/compiler5.04.00.2904 and D3DX warning strings;
that proves bundled D3DX code exists, not yet an exact attribution of these
two individual functions. Do not turn that lead into a fake completed conversion.
