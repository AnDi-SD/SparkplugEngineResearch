# PC material pass update and texture binding — CP34–35

Original PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
No real GPU, renderer constructor, OS forwarding or capped-path retry.

## Pass update: original classes, original traversal

Actual45F570 protected bridge reaches4596B0; the probe checks that visited
address on every call. Pass+14 is count, slots+18 supply owned texture layers.
FFFFFFFF stage means each layer ordinal0..count-1; any other requested stage
is passed unchanged to **every** layer.423460 takes layer+10 and tail-calls
MaterialTexture467B70. Thus existing UV-before-AnimTex ordering is reused,
not reimplemented in a parallel pass animation engine.

Source `spMaterialPassLayer` and `spMaterialTextureLayer` now expose the
analytically named `UpdateForRenderForAnalysis`, delegating to the same
material-texture consumer. Original wrappers return void; source bool is
a host rejection of NULL/sparse input, not a recovered return contract.
Empty pass succeeds; source refuses NULL slots that native code dereferences.

Six exact cases: empty, two-layer auto stage, forced0, forced1, eight layers
without UV, failed device. Original actual layer call order is independently
asserted; exact source captures compare emitted4x4 matrices at the COM call,
including two different layers writing the same stage. No after-the-fact
pointer dereference that would lose the first matrix. **18 counted native
checks**, max1935 instructions/63,632 arena. Initial source suite76/76,
build52/52, CTest40/40 (30.76s). Final bridge-assert regression6/6 exact.

## Texture identity cache and palette selection

Actual4BB650 consults global75526C debug-object byte1F; nonzero replaces
the requested texture with NULL. Cache `E480+4*stage` compares **engine
texture identity**, not COM handle. Changed non-NULL DX texture3F3651B6
submits its+3C handle to COM+104, obtains palette through primary+38, calls
renderer+50/4BB1C0, then stores the engine identity. Changed NULL submits
NULL but does not select/reset palette. An identical identity skips both.

4BB1C0 ignores NULL palette. Otherwise it compares palette+10 with global
cacheCA0C, stores the new index **before** COM+124, and returns true even on
failure. Hence palette call sees the new palette index but the old texture
identity; texture call sees both old caches. Same texture at another stage
does not repeat an already selected global palette.

Five exact cases: plain, palette, failed HRESULT, toggled debug suppression,
direct palette selection with repeated/NULL/change. Actual DXTexture factory,
RTTI branch, palette getter, renderer selection and destructors execute;
external COM objects/handles are declared inputs, no texture upload claim.
Device and texture Release balance independently checked. **51 counted native
checks**, max77 instructions/63,760 arena. First scout cleanup omitted the
resource manager created by the texture factory; explicit teardown corrected
that fixture assertion, without a cap or internal behavior substitution.

Source `spDXRenderer::BindResolvedTextureForAnalysis` covers only the
identity/cache/COM portion with explicit resolved opaque identity/handle
and optional palette index. It does **not** pretend to construct live COM
textures or support the unresearched alternate RTTI branches5C542AD9 and
5D982205. `SelectPaletteForAnalysis` is the common original-order cache
equivalent. Missing device callbacks are explicit host guards.

## Combined validation and next boundaries

Final source material application suite **123/123**, binding build51/51,
full CTest **40/40**,26.82s. Unique profile cases6+5=**11 exact**, counted
native checks18+51=**69**; pass regression is not additional unique coverage.
Run records in `local-data/results/bounded-native-runs/`:
`20260907T010223369748Z-pc-material-pass-update.json`,
`20260907T010917413309Z-pc-material-pass-update.json`,
`20260907T010835568700Z-pc-texture-binding.json`.
Limits unchanged100k/2s call,30s child,64KiB arena,32KiB native request.

PC PassLayer70→76, TextureLayer72→74, DXRenderer57→58. PS2 unchanged.
No full class/completion gate is100%. Next4BBBA0/4BC410 pass-state selection,
then4BC290 draw and4BC4A0 complete submission. Independent one-layer pass
scout succeeds but is not source parity/score in this checkpoint.

Static connected follow-up:4BC290 uses E454[E474] as a vertex shader object
pointer (+50 passed to COM+170), and E464[E478] similarly for pixel shader
COM+1AC. Thus CP31's nonzero coordinate override is not simply an arbitrary
hardware boolean. Push4BE1B0/4BE1E0 and default generation4BE310 remain to be
executed/described; this static observation does not close shader lifecycle.
