# PC Skin: complete light submission in decoded draw

CP86,2026-09-07. Original pinned WinxClub.exe SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
16 exact captures,1213 native assertions (1069 linked and144 shared draw).
Limits unchanged:64KiB arena,32KiB request,100k instructions/2s per original
call,30s fresh child. No OS/GPU forwarding or substituted engine success.

## Connected path

The actual owning Skin/Material/bone reader70998 instructions, real
bbush.san, SceneManager world373 and decoded native DXMesh are retained
from CP76. Material state8 changes in the encoded input. Actual DXLight
factory4AC000 creates158 bytes; original4B53C0 builds its26-word device
payload from explicit color/intensity/range/angles and prepared world cache.
Original4BC4A0 calls4BDE50 whenever material38 is not2, after material
state application and before device material submission/pass iteration.
The same actual light payload reaches COMCC and enableD4 before the cached
automatic shader key/constant/indexed draw path.

Cases:NULL list, empty list, directional/point/spot, disabled ordinary,
ambient-only, negative device HRESULT, false post callback, and all raw
material lighting modes0..7 (unlit-list covers2). Source and original agree
on complete device events, raw material colors, group/cache/count state,
light payload, SAN/scene palette, mesh bytes, shader key and draw output.
Render15315..15713 instructions,peak64312 bytes,99 freed engine owner
generations. Original light world producer/scene light selection are still
prepared inputs; this is not a full Scene frame or light asset loader.

Disabled ordinary lights still contribute their count/type to4BE310's
shader key. A non-NULL light list also contributes when raw material mode2
skips4BDE50 entirely. NULL submission disables8 slots and preserves old
global count/group pointers; an empty non-NULL list clears both. Existing
common SubmitLights logic now runs inside source whole geometry submission.

## Two source gaps found by composition

1.4BDB10 modes1/6 convert the current global73FE98 through424700. This is
the same global used for fallback ambient by4BDE50. Its usual value is
opaque black, but it is mutable input, not a hardcoded constant. Testing
7F234567 exposed source's hardcoded black. LightingState now carries the
global value, connected to the same fallback input in whole submission.
CP32's default-black evidence remains valid; it was not general evidence
that black is immutable. Mode0's white is immediate1.0, not that global.
2.Mode6 uses current C194 populated by Skin pre. Source now connects that
selected Skin color to material lighting as well as shader constants.
The case uses80402010, proving all RGBA channels and avoiding a default-only
coincidence. Missing propagation previously produced a different alpha.

Source exposes `spSkin::RenderForAnalysis` and
`spDXRenderer::SubmitGeometryForAnalysis`; earlier unlit API names remain
compatibility facades. Light list/state and external callbacks are explicit
carriers, not invented original RTTI classes. No shader lighting constants,
first lit shader generation, light-camera world inheritance, malformed
lists or real GPU output are claimed by these four-register captures.

Validation:

- `20260907T131321113032Z-pc-skin-lights-render.json`:16/16 exact.
- `20260907T131332012079Z-pc-skin-queued-mesh-render.json`:CP85 regression3/3.
- `20260907T131147068612Z-pc-material-lighting.json`:CP32 regression7/7.
- Full build/CTest60/60 (54.11s) before the final C194 bridge; the final
  focused profile above verifies that bridge and all16 composed cases.

Reports live under `local-data/results/bounded-native-runs/`.
