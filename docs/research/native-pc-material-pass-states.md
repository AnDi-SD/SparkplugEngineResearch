# PC complete pass texture-state batch — CP36

Original PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual4BBBA0, and4BC410 final blend after that helper. Original material/pass/
StdLayer/MaterialTexture factories and virtual state/getter calls execute;
only external device COM is observed, never actual GPU/OS.

## Prepare all eight texture stages

If `E454[E474]` is nonzero, compare each stage's mapped UV caches first:
nonzeroE954 invalidates raw state8 toFFFFFFFF; E920 differing from stage
invalidates raw state7. Then selected materialC18C/pass[argument] supplies
its layers. Their MaterialTexture virtual getter supplies desired texture
pointerC198; changed identity sets byteC1B8. Layer slot7 copies nine raw
state words into desired blockC29C (stage stride24 hex).

Unused stages clear desired texture pointers and mark dirty if changed.
Their nine desired words come from default materialC9C0, pass0: layer0 for
stage0, **layer1 for every higher stage**, independent of active layer count.
Default textures themselves are not bound; their state words alone are used.

All dirty texture bindings execute first, in stage order0..7, through actual
4BB650; dirty flag is cleared afterward. Only then does the state batch
visit each stage0..7 and each state1..8. Raw state0 is never applied.
Thus this outer raw cache suppresses repeated states1..6 even though the
inner4BB1F0 mapping itself always submits them. The two layers of caching
must not be collapsed into one assumed policy.

## Overrides and final blend

Selector `C748+4*(9*stage+state)` chooses one complete72-word desired-state
block startingC29C. Source block0 is the freshly prepared pass; alternatives
are explicit caller-provided blocks. This lookup is used by4BBBA0 regardless
of material override byteC1C4. No unsupported selector extent is invented.

4BC410 first executes4BBBA0, then writes pass+10 final blend to **material
state7 itself**. If C1C4 set, selectorC738 instead selects the desired raw
blend from material-state source pointersC1C8; writing the material still
happens even with override. Only changed raw blend calls4B0AD0(index7).
Original HRESULTs are ignored throughout these wrappers.

## Reconstruction and proof scope

`spDXRenderer::ApplyPassTextureStatesForAnalysis` operates on original
`spMaterialPassLayer`/`spMaterialTextureLayer`/`spMaterialTexture`, then calls
the common checked binding and state-mapping slices. Resolved non-NULL
device inputs must match actual texture identity; mismatches are refused.
This checkpoint's combined native tests use **NULL texture references**,
plus old cache tokens that are compared/cleared but never dereferenced.
CP35 separately proves real DXTexture binding; non-NULL full-pass integration
is still to be tested, not silently credited. `ApplyPassBlendForAnalysis`
preserves the selected material mutation and override/device-cache ordering.

Eight exact cases: empty/one/two layers, texture-source overrides, shader
coordinate override, failed device, final blend, final blend override.
Three iterations each with repeat and changed input. Exact captures compare
desired72 words, raw72 words, sixteen UV device cache entries, dirty flags,
material/raw blend and every intermediate raw72 snapshot at COM calls.
**32 counted native checks**, max6486 instructions/63,568 arena, all actual
allocations freed. Source **160/160**, build51/51, full CTest**40/40**,25.42s.
Run `local-data/results/bounded-native-runs/20260907T011744382798Z-pc-material-pass-states.json`.
Limits unchanged100k/2s call,30s child,64KiB arena,32KiB request.

PC DXRenderer58→62. No PS2 transfer/full class/gate completion.
Next4BC290 shader constants/draw and shader selection, then enclosing
4BC4A0 buffer/material/pass submission. Full startup, reset, GPU resource
creation, nonstandard layers, whole-file save/display remain open.
