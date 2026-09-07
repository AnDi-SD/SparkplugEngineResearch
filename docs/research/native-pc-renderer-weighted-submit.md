# PC complete cached weighted submission — CP48

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
[CP42](native-pc-renderer-submit.md)'s complete4BC4A0 is now exercised with
actual weighted cached shader selection, not just no-weight automatic draw.
Original buffer/declaration binding →4BE180 material install →4BB890 state
set →45F570 layer update →4BC410 pass state →4BC290 →4BE310/4C8980 →
4BE2B0/4AE930/4BE210 →COM constants/draw all run without internal seams.
The matrix case also executes original dirty4AD540 through the descriptor.

Fixture state8=2 remains explicitly unlit; flags0x803; actual manager cache
key(0x20011,0), reflecting raw lighting/color mode2. Real PCVertexShader
owns type5(count3)+type8(count1), plus type1(count4) for matrix case.
Four calls change bone/diffuse inputs; matrix case changes all3inputs and
dirty each time. One/two passes and failed HRESULT checked. Existing NULL
COM buffer/shader handles are declared external inputs, not uploaded assets.
Automatic selected shader cleared after each draw, bound identity persists;
each pass evaluates updated constants, and matrices use the shared dirty cache.

Source extends existing `SubmitUnlitGeometryForAnalysis` with optional explicit
constant inputs, using common `DrawCachedAutomaticForAnalysis`. No duplicate
alternate pipeline. Material identity must match its constant input; no-weight
callers retain existing API/default behavior. Generating miss returns incomplete.
Lit state, unknown/non-NULL texture resolution and preselected full submission
remain refused by this wrapper, not silently simulated as success.

`pc-renderer-weighted-submit`:4/4 exact20 native; original no-weight6/6
exact30 native regressed with the same current wrapper. Source136/136,
build69/69,CTest50/50 in39.58s. Reports:
`local-data/results/bounded-native-runs/20260907T034811465713Z-pc-renderer-weighted-submit.json`
and `20260907T034822540635Z-pc-renderer-submit.json` in that directory.
New modes max15250instructions/64640arena. Previous100k/2s per call,30s
child,64KiB arena,32KiB request limits unchanged. No native cap or fixture
failure in this checkpoint. No asset writes, device/OS forwarding or release.

PC DXRenderer76→78; no PS2 credit or class/gate closure. Remaining lit/
textured/whole-scene integration, descriptor/compiler production, full renderer
lifetime/reset, source mesh adapter and real GPU display still required.
