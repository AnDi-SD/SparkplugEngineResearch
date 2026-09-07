# PC texture/sampler state mapping — CP31

Original PC4BB1F0, pristine EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual native internal dispatch; only external COM+10C/+114 are observers.
No GPU/renderer constructor/reset. Source stays in `spDXRenderer`.

## Storage and tables

Raw common cache is `C898+4*(9*stage+index)`, written before dispatch. Input
stage0/1/7 stays inside the known72-word common cache. Source guards stages
outside0..7 and indices outside0..8 instead of emulating native OOB writes.
Index0 has no command. Indices1/2 require value0..15 for the unbounded native
lookup; source rejects larger values after the raw-cache write.

| Raw state | Original translation |
|---:|---|
|1|COM stage-state1, table6F19D4 with stride8|
|2|COM stage-state4, same table|
|3/4|COM sampler-state1/2; value0→1,1→2,anything else→3|
|5|COM sampler-state4, unchanged raw word|
|6|COM sampler7,5,6 in that order; see filter mapping below|
|7|COM stage11 with coordinate-generation mapping, separate device cache|
|8|COM stage24 with transform flags, separate device cache|

Operations0..15 map to `[1,2,3,4,5,6,7,10,13,12,15,16,18,19,20,21]`.
Filter0→(0,1,1),1→(1,1,1),2→(2,2,2),3→(2,3,3),other→(0,1,1), in
sampler7/5/6 order. All these states1..6 **always submit**, even on identical
repeat. Every HRESULT is ignored. Do not reuse render-state dedup globally.

## UV-specific cache and override

State7 values0..7 map unchanged;8→10000,9→30000,10→20000,11→30000,
12→40000,other→0. Device cache is `E920+100*stage` (hex offsets).
State8 starts with `value&7`;0..4 remain unchanged,5..7 use **stage**, not
zero. Bit8 adds100. Device cache is `E954+100*stage`. High bits ignored.
Both UV states compare their separate cache, call COM only if changed, then
write device cache after the call. Observers see updated raw state and old
device cache. Failed HRESULT still suppresses an identical subsequent retry.

If `E454[E474]!=0`, state7 instead stores **stage in both raw and device
cache**, whereas state8 maps to0 but keeps its original raw word. Test modes
named `hardware*` are analytical shorthand only: the original selector/flag
name and full ownership/setter meaning are not inferred from this branch.
Tests declare selector3 and a nonzero flag there; no selector bounds claim.

## Reconstructed slice and evidence

`TextureStageCacheForAnalysis` exposes exactly the caller-provided raw9 and
two mapped UV entries, not a guessed full native backend object/default.
`ApplyTextureStateForAnalysis` preserves command ordering, per-state dedup,
raw override and ignored HRESULT. Host NULL callback/OOB guards are explicit
safety differences. No class or completion gate is declared100%.

Profile `pc-texture-state-map`: **9/9 exact captures,1359 native assertions**.
Groups ops,address,border,filter,uv-index,uv-flags,override-index,override-flags,
failed device; three stages0/1/7 and repeated calls. Original maximum55
instructions per call,arena62,368; all actual native allocations freed.
Bounds unchanged100k/2s call,30s child,64KiB arena,32KiB native request.

`SparkplugDXRenderStateTests`: **1631/1631**;build49/49;full CTest**39/39**,
42.65s. Run record
`local-data/results/bounded-native-runs/20260907T003138251003Z-pc-texture-state-map.json`.
Native/compare `pc_texture_state_map.py`, common COM fixture from
`probe_pc_material_apply.py`, source `spDXRenderer.*` and existing state test
target. PC DXRenderer49→53; no PS2 score transfer.

Next connected boundary:4BDB10 lighting/color-source choice, then actual
material/pass application and mesh submission. Initial independent modes/
source-selection/power-NaN native scouts succeed but are not part of this
checkpoint's source parity or score. Whole-file Save/reset/display remain open.
