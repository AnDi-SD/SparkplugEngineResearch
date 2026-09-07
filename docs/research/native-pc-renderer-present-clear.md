# PC Clear / Present boundary — CP28

Pristine PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original internal instructions execute; COM methods are external observers.
No GPU, native renderer startup, or device reset is executed.

## Clear4BB980

Secondary `this=complete+18`; arguments flags, ARGB, stencil. Reads the low
flag byte and maps only bits0..2. Calls device complete+C9E8, COM slot+AC,
with rectangle count0, NULL rectangles, `flags&7`, unchanged ARGB, float
depth1.0, unchanged stencil. Returns AL1 even for a failing HRESULT.
All256 low-byte combinations plus100/107/80000000/FFFFFFFF are tested both
with success and failure HRESULTs:260 inputs per native child.

## Present4BB9F0

Complete+C1C0 nonzero returns AL0 without touching the device. Otherwise
COM slot44 receives four NULL arguments. Any HRESULT except88760868
(DEVICELOST), including generic failure, leads to AL1. DEVICELOST calls
COM slot0C (TestCooperativeLevel). Only88760869 (DEVICENOTRESET) follows
the internal reset path; other cooperative results return AL1.

The reset path copies32 bytes complete+1C..3B onto the stack and prepares
secondary vtable slot8/+20=4BD810 with a pointer to that copy. The test
**stops before the call instruction4BBA50**, checks the actual registers,
stack and copied bytes, and never resumes. This is not a successful native
Present/reset execution. The reset helper and device/resource rebuilding
remain unknown. The wrapper's one stack argument is not read in this body.

Portable `PresentBeforeResetForAnalysis` returns an explicit incomplete/
reset-required boundary instead of inventing reset behavior. A NULL external
callback is a host guard, not a native behavior claim. Clear and Present APIs
use analysis names and do not imply a complete native renderer layout.

## Verification

`pc-renderer-present-clear`: **7/7 exact source/native captures,1062 native
assertions**. Modes clear, clear-fail, present, present-fail, blocked, lost,
lost-reset-stop. `SparkplugRendererSceneTests`: **556/556**; build48/48;
full CTest**38/38**,36.98 seconds. Native maximum43 instructions per call,
arena52,000 bytes;100k instructions/2s per call,30s child,64KiB arena and
32KiB native request limits unchanged. All actual native allocations freed.

Persisted schema2 run record:
`local-data/results/bounded-native-runs/20260906T235117673692Z-pc-renderer-present-clear.json`.
Native/compare scripts: `research/probe_pc_renderer_present_clear.py` and
`research/compare_pc_renderer_present_clear.py`. Source:
`Sparkplug/Code/SparkplugDX/spDXRenderer.*`, `Sparkplug/Tests/spRendererSceneTests.cpp`.

PC DXRenderer44→46; no PS2, reset, whole-display or class100 credit.
Next connected material boundary: DXShaderLayer71643E66, which common
standard-layer loading does not yet support.
