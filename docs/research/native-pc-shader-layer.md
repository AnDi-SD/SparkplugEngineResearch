# PC spDXShaderLayer — CP29

Pristine PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Class71643E66, direct base spMaterialTextureLayer7F577C6D; record763270,
initializer6D4DA0, registration call6D4DC0. Original header/source/API spelling
not recovered; `Code/SparkplugDX` is explicitly inferred placement.

## Actual factory, storage and ownership

Protected factory4ABDC0 succeeds in the unchanged bounded emulator, allocation
**28 bytes**. Vtable6F0B04:
`4B3200,5B7A00,4AC060,4B2FD0,4B31B0,408350,408370,423590,4D74A0,4F3DF0`.

- +10 is the inherited direct-owned material-texture pointer, initiallyNULL.
  Unlike StdLayer, this factory does **not** create a nested MaterialTexture.
- +14 is untouched. Later copy/reader treats it as a borrowed shader word;
  no retain/release observed. CC fixture poison is not a real default.
- +18 allocator/helper word is untouched; +1C/+20/+24 are zeroed vector
  begin/end/capacity pointers. Elements are16 bytes: two `(count,pointer)`
  pairs, each pointing at `count` raw float4 rows (16 bytes per row).
- 4B1230 appends a pair descriptor. Clear4B2F60 frees both arrays of every
  active pair **and the vector buffer**, then zeros all three pointers.
  Destructor4B31C0/4B3200 calls clear then inherited423530 direct texture
  destruction. The borrowed14 token is not dereferenced/deleted.

Actual clone4AC060 creates/registers a new object, calls copy4B2FD0. Copy
clears destination, invokes inherited4235E0, copies borrowed14 unchanged,
then deep-copies both arrays of each parameter pair. Tests use zero, one,
three pairs, distinct counts1..3 and arbitrary raw float words. A separate
real nested MaterialTexture clone succeeds. Source mutation does not change
the clone. All tracked native allocations are freed.

Virtual slots8/9 (4D74A0/4F3DF0) are success-only stubs, accepting two/no
stack arguments. Reader helper5A7DB0 is also a one-argument success stub.
Inherited slot7/423590 requires a non-NULL texture and writable nine-word
output; a factory-empty shader layer does not establish that precondition.

## Negative codec finding — do not bypass the base check

Actual DXMaterialSerializer layer read4B12A0, write4B0F10 and index4B0EB0
first call common477230/477350/4767F0 respectively. Those common functions
accept specific Std/Env/Cube/Camera/Mirror/Movie RTTI families, not the base
MaterialTextureLayer. Exact DXShaderLayer has none of those bases.

With the **correct PC RTTI parent chain**, all three return false before
stream consumption/output or shader-manager work. These native rejections
are executed, not inferred from a missing host implementation. It would be
incorrect to skip the common check to make the following branch reachable.
This is a finding about these actual helper entries/identity, not proof that
no alternative runtime construction/processing path exists anywhere.

Unreached static tail4B12F8 onward reads a **u32 key** via416DA0 (not a string),
uses manager763024 (factory4C9680, lookup4AFFB0/add4B0680), and a resource
reference with expected ID394F7331. It stores layer14, reads a discarded
scalar via5A7DB0, processes material texture476890 and parameter arrays.
Do not credit this unexecuted tail as a functioning native shader codec.

## Source / verification / limitations

`spDXShaderLayer` is reconstructed with the same RTTI/base, explicit optional
raw14 word (unset is **unknown**, notNULL), deep-owned parameter arrays and
the inherited direct-owned texture. Registered source factory is now backed
by actual native factory evidence. Source rejects self-copy as a host guard;
native alias/exception/malformed/zero-count allocation variants remain open.
No shader loading/compilation/COM object or working shader file path invented.

`pc-shader-layer`: **8/8 bounded children,55 native assertions,7 exact
source/native captures** plus separate native-only rejection. Source suite
**36/36**, full CTest**39/39**,35.17s. Initial compile used a C++20 comparison
default in this C++17 project; replaced by explicit equality before successful
214-step remaining build (original239-step build stopped at26). No source
comparisons run during linking. An initial clear test wrongly expected buffer
reuse; observed actual freed buffer/null pointers corrected that expectation.
An early declared RTTI base ID typo was corrected to415352A1; final profile
uses the exact chain. No instruction/allocation cap reached or raised.

Native maximum10,018 instructions,arena912 bytes.30s child/100k instructions/
2s call/64KiB arena/32KiB allocation bounds unchanged. Report:
`local-data/results/bounded-native-runs/20260907T001256924257Z-pc-shader-layer.json`.
Scripts `probe_pc_shader_layer.py`/`compare_pc_shader_layer.py`, source tests
`Sparkplug/Tests/spShaderLayerTests.cpp`. PC new assessment65; serializer55
retained with negative evidence. No PS2/class100/whole-display/gate credit.
