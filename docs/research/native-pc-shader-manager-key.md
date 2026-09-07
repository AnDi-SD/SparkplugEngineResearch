# PC shader manager and renderer key — CP38

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original source path at6F2C58 is
`Z:\Sparkplug\Code\SparkplugPC\spPCShaderManager.cpp`.
ClassD5AE63DA, registration764CB0/initializer6D5A00, direct base
spDXShaderManager98BA76FE/763A80 (abstract registration), base415352A1.
Header and analytical method names are inferred; namespace is portable.

## Manager lifetime and cache

Actual protected4C9680 factory succeeds under unchanged limits: size0x50,
primary6F2DE4 and secondary+10=6F2DE0. Empty vectors18/1C/20 and28/2C/30,
map root38/count3C, NULL template40, pair-key map root48/count4C.
Allocator words14/24/34/44 are untouched, not fabricated pointer defaults.
Primary slots:4C9660,5B7A00,4C96E0,40ECE0,4C9480,408350,408370,
4C8980,4C97E0,4C8F10. The next words are a string, not more virtual slots.

4C8980(mask,lightTypes) first checks `mask & 15`. Zero returnsNULL without
even looking at the cache, including when an entry for that key exists.
Otherwise4C80F0 finds the pair. Node+14 is the returned shader pointer.
Cache miss enters source generation/compilation; it is NOT a normal NULL hit.
The source `SelectCachedForAnalysis` explicitly returns completed=false on
such a miss. No internal compilation function is replaced by fake success.

4C7F40/4C80F0 and actual protected insertion4C87A0 compare **eight raw
little-endian bytes lexicographically**, not two numeric integers or u64.
Proof includes keys256 versus1, and second words256 versus1: native ordered
keys are `(256,0),(1,256),(1,1),(257,0),(257,1),(2,0)`.
The probe inserts actual uncompiled PCVertexShader objects via4C9F10.
This tests cache identity/ownership, not shader validity or compilation.
Portable map owns actual reconstructed spPCVertexShader objects.

Clone4C96E0 registers an actual clone and calls40ECE0: new **empty** manager,
not a copy of cache/template/vectors.4C9370 destroys template40 if present,
direct-deletes each non-NULL cache value, destroys nodes/root, then4B0740
destroys both owned shader vectors and base map.4B0740 unconditionally
clears763024, even if its value no longer equals this manager. Portable
source deliberately has no process-global renderer/manager singleton;
that lifetime side effect is documented, not claimed as implemented.
Populated first/second vectors, template object and base name-map ownership
still need independent execution; cache destruction is fully exercised.

## Renderer key, original4BE310

The bounded prefix executes through original material power getter and actual
manager factory, stopping **before** indirect call4BE495, never resuming it.
Separate no-weight cases execute the whole4BE310→4C8980 and returnNULL.
Native source-generation format strings give real parameter names:

| Key portion | Original meaning and construction |
|---|---|
|bits0..3|BlendWeightCount: highest component bit10/8/4/2 gives4/3/2/1|
|bits4..7|texture-coordinate count: highest component bit40000..800 gives8..1|
|bits8..15|bHasUVTransform[0..7]: raw state8 atC8B8+24hex*stage has bit2|
|bits16..19|ColorMode; construction actually ORs **raw C888<<16**, no mask|
|bits20..23|LightCount from lightsC190+24, shifted20, no count mask|
|bit24|bUseSpecular iff selected materialE47C adjusted+14 getter+28 returns >0|
|second word|OR raw light+C0 types shifted2*i, without masking each type|

Zero/negative/NaN power clear specular; positive subnormal and +infinity set
it. Raw out-of-range color/type inputs preserve overlapping bits exactly.
Source limits explicit light arrays to8; native has no such loop guard.
These are consumer inputs, not a claim that renderer startup permits all
raw combinations. No PC observations are transferred to PS2.

## Open connected work

4C97E0 loads `Shaders\Fixed.rfx` after base4B0890 scans shader files.
4C8980 creates original parameter literal strings and calls4CFFE0 on the
template object; then shader+1C receives generated code.4C8F10 also reaches
611241. Script/parser/compiler implementations are not bypassed or complete.
Original Fixed.rfx exists in the pristine PC corpus; its HLSL exposes matching
BlendWeightCount/ColorMode/LightType/UVTransform names. This is supporting
resource evidence, not proof that the native compiler path already works.

Native manager probes: factory8, no-shader8, cache26, clone28 counted checks;
key groups components25,colors13,lights12,uv262,power10,fixed9. **10/10 exact
captures,401 counted native checks**, source363/363, build250/250,
CTest42/42 in34.05s. Reports:
`local-data/results/bounded-native-runs/20260907T015802751037Z-pc-shader-manager.json`
and `local-data/results/bounded-native-runs/20260907T015815285304Z-pc-shader-key.json`.
Manager≤8282 instructions/1280 arena; key≤6799/64,416. Source vertex-shader
foundation is present for actual cache ownership; its separate device/copy
tests and parameter semantics are the next checkpoint, not credited here.
Limits remain100k instructions/2s per call,30s child,64KiB arena,32KiB native
allocation. No game/OS/GPU forwarding, no asset modification.
