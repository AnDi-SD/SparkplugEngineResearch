# PC material lighting and color sources — CP32

Original PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual `4B0AD0(index8)` → primary slot35/`4BDB10`, and direct color-source
setters `4BDDB0`/`4BDE00`. Only external COM+E4 is an observer. No full
renderer construction, GPU, OS forwarding, or internal fake-success seams.

## Exact consumer rules

The raw mode at complete renderer+C888 is written first. Every lighting
call next dispatches device state29: true only for modes3..5 and
`float(E4E4)>0`. Zero, negative and quiet NaN produce false; positive infinity
produces true. Values above7 then return success without changing the other
colors/sources/lighting. Device HRESULT never controls success.

The17-word material block is diffuseE4A4, ambientE4B4, specularE4C4,
emissiveE4D4, powerE4E4. The following are raw native mode values, not
recovered original enum names:

| Mode | Color mutation | Device lighting137 | Diffuse/ambient source |
|---:|---|---:|---|
|0|Emissive white|1|10/10|
|1|Emissive receives old diffuse; other colors black; preserve old diffuse alpha|1|10/10|
|2|Unchanged|0|11/10|
|3|Unchanged|1|10/10|
|4|Unchanged|1|10/11|
|5|Unchanged|1|11/10|
|6|Emissive receives ARGB C194; other colors black, alpha1|1|10/10|
|7|Unchanged|1|10/10|

Black is (0,0,0,1), white (1,1,1,1). Source setters cache raw values at
E4E8/E4EC before dispatch, suppress identical raw values, map10/11/12 to
0/1/2 for device states145/148. Other values still update raw cache but do
not submit. The lower `4B0A90` cache can independently suppress a command.

ARGB conversion `4A9200` (and wrapper424700) uses the actual float32
coefficient at6DCA9C, `0.003921568859368563`, in R/G/B/A order. It is shared
with the already restored material-color controller through
`Analysis/PC/spColorMath.h`; no double-precision replacement coefficient.

## Reconstruction and bounded proof

`spDXRenderer::LightingStateForAnalysis` is explicitly supplied consumer
state, not recovered constructor defaults. State8 without that input is a
host rejection, not a fake successful native path. Source remains in the
same renderer and calls the already checked common device-cache helper.

`pc-material-lighting`: **7/7 exact captures,103 native assertions**. Compare
every raw color/power word, source cache and intermediate state observed at
each device call, not merely final output. Modes0..8/FFFFFFFF, repeats,
source10/11/12/9/13/FFFFFFFF, failed HRESULT, zero/negative/NaN/infinite
power. Native max218 instructions, arena62,368; all tracked allocations
freed. Bounds unchanged100k/2s call,30s child,64KiB arena,32KiB request.

Build50/50; source state tests **1727/1727**; full CTest **39/39**,28.98s.
Shared ARGB change additionally rechecked `pc-material-color` **10/10 exact**.
Run records `20260907T004412203011Z-pc-material-lighting.json` and
`20260907T004508121330Z-pc-material-color.json` in
`local-data/results/bounded-native-runs/`.

PC DXRenderer53→55. PS2 unchanged. Original field/enum names, full startup,
material/pass/mesh submission, reset, whole-file save/display and malformed
inputs remain open. Next connected boundary is4BE180 material installation
and4BB890 state application; static copy-before-color-update ordering is
not yet claimed dynamically at this checkpoint.
