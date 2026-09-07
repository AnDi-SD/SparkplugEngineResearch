# PC texture: runtime flat codec, native mip copy и настоящий registry key

Checkpoint15 продолжающегося PC SMO/SAN этапа. Оригинальный EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [CPU/COM boundaries](native-pc-texture-codec-boundaries.md).
Ни game/GPU, ни сторонние APIs не запускаются. Source — CPU shadow,
не live Direct3D texture;100% readiness не заявляется.

## Разные wire, runtime и serializer identifiers

Actual startup entries `6D1850/6D1880`, `6D18B0/6D18E0`, `6D1910/6D1940`
исполнены на явно пустом manager, затем original4224F0 выбрал reader/writer.
Все шесть registers **один wire ID `78EA082B` (TextureData)**:

| platform mask | operation masks | Factory/vtable | virtual slot28 value |
|---:|---:|---|---:|
|6|2 и1|DXData42B660 /6DD648|0B1C67BB|
|8|2 и1|PS2Data42C9E0 /6DDBA8|24767C83|
|1|2 и1|Data42DC30 /6DDD90|78EA082B|

Для manager platform2 или4 selected DXData; для1 — Data; для8 — PS2Data.
**Виртуальное значение serializer не является его единственным registry key.**
Lookup0B1C67BB после настоящих шести registrations возвращаетNULL.
Прежняя подпись «target=spDXTextureData» не доказывает существование отдельного
registered runtime класса с этим ID. `spDXTexture3F3651B6` — отдельная запись.
21 checks, manager уничтожает все созданные serializers/nodes.
PS2-masked registrations исполнялись в **PC EXE**, PS2 score не повышается.

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

Eight1×1 formats and six2×2/4×4 full chains passed original execution:
141 assertions,14 exact source comparisons. Read≤601 instructions,
write≤579, arena≤53008 bytes for these tiny fixtures. Palette-free cases only.

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

Nine cases:four1×1 formats plus raw2/raw4/DXT1-4/DXT3-2/DXT5-4.
94 assertions, read≤16233 instructions/54608 arena bytes. All temporary
TextureData buffers/vector/objects and final COM owners balance in fixtures.
This is native row-copy completion, not mip generation, error rollback or GPU.

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

Source test suite451 checks including CPU regression,14 runtime rows, malformed
inputs/pitch, actual Data/DXData header identities, generic runtime header.
Fresh sequential build completed after correcting test typo IsTypeOf→existing
IsKindOf; no native behavior changed for that compiler error.
Full CTest32/32,23.96sec. No cap exceeded or retried.
Profile `pc-texture-runtime-mips`24/24:14 runtime comparisons,9 nativeData
cases,1 startup registry. Всего256native assertions. Workbench10/platform12
unit tests и diff-check проходят. NativeData reader пока native-only, не
выдаётся за перенесённый source adapter.

Later checkpoint: [shared native-data source reader](native-pc-texture-native-source.md)
implements and verifies eleven native-data cases, including recursive field 3.
The native-only/source-pending statements above describe this checkpoint's state.

## Real-resource lead and unclosed dependencies

DB corpus2 contains actual `TextureData78EA082B`; it does not require guessing
0B1C67BB runtime records. `Characters/Animals/icebat.smo`:
35571 bytes, SHA256 `6AEC9CA21EB50FD93E89955551C3557FF19BD21260038FF6E7CF94EEF178BF72`.
Texture object physical1336,size4160, header78EA082B/SBOO; outer field3 embeds
another source/local section with platform6 and native field1. Only bytes/static
framing were inspected here, **not native whole icebat load**.
`42EA50` field3 calls the same serializer's virtual secondary read recursively
at42EC2D, then marks source handled. It is not automatically a new file/FFPS header.

`60FDB4/61039A` internal library bodies remain unclosed. The EXE does contain
explicit D3DX9 shader assembler/compiler5.04.00.2904 and D3DX warning strings;
that proves bundled D3DX code exists, not yet an exact attribution of these
two individual functions. Do not turn that lead into a fake completed conversion.

Palette object is now identified by actual registration: **spPalette591C0B9F**,
directBase415352A1, record763DE0, initializer6D52E0, factory4B2C80,
constructor4B2C10, copy-constructor4B2C50. Allocation414 observed statically
at runtime reader; full factory/lifetime/renderer palette registration still to probe.
DX+40 setter4B93C0 deletes/replaces prior palette, but complete destructor/registry
policy is open. Source palette stays unsupported until this dependency is closed.

Next: nativeData/source field3 adapter, real smaller asset slice, shared
MaterialTexture ref graph, palette, missing-mip/conversion, COM error/reuse/clone,
whole FFPS Save and actual resource→render integration. No PS2 credit, application
or game asset mutations, commits/publication. Checkpoint is not goal completion.
