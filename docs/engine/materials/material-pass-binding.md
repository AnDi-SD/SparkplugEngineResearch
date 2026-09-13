# PC material pass update and texture binding

## Pass update: original classes, original traversal

Source `spMaterialPassLayer` and `spMaterialTextureLayer` now expose the
analytically named `UpdateForRenderForAnalysis`, delegating to the same
material-texture consumer. Original wrappers return void; source bool is
a host rejection of NULL/sparse input, not a recovered return contract.
Empty pass succeeds; source refuses NULL slots that native code dereferences.

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

Source `spDXRenderer::BindResolvedTextureForAnalysis` covers only the
identity/cache/COM portion with explicit resolved opaque identity/handle
and optional palette index. It does **not** pretend to construct live COM
textures or support the unresearched alternate RTTI branches5C542AD9 and
5D982205. `SelectPaletteForAnalysis` is the common original-order cache
equivalent. Missing device callbacks are explicit host guards.
