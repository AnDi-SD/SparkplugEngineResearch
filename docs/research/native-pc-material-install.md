# PC material installation and state batch — CP33

Original PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual4BE180 and4BB890 on bounded caller-declared renderer state. Material
objects use actual4A9460 factory/destructor. Color-controller backing is
the explicit CP25 consumer fixture: capped41A580/41AF70 remain forbidden.

## Installation before animated color update

4BE180 stores a **borrowed** material pointer atE47C, copies17 words from
material+78 to renderer+E4A4, then calls material's adjusted+14 interface
slot0 with force0 (`4A9530`). It always returnsAL1; no COM call here.
Thus the installed color block is the material's **pre-update** state.
This ordering is verified on none/animated/shared controllers, same-frame
repetition and consumed clocks. It is not silently changed to update first.
The shared controller still updates only its last-bound material.

Source `InstallMaterialForAnalysis` calls the existing DX material frame
consumer after copying. NULL material and uninitialized native power are
explicit host refusals. Renderer borrowed pointer acquires no ownership.
The fixture's artificial frame sequence does not claim full game scheduling.

## Material states and override table

4BB890 refreshes `C1C8 = C18C+18` (selected material's state array), then
unconditionally sets raw lighting cacheC888 to255. It visits indices1..10
in order, calling actual primary+78/4B0AD0 only for unequal raw values.
Lighting therefore normally reexecutes every application, although lower
device-state cache may emit no commands. Raw index0 is not applied.

With byteC1C4 clear, desired state is `C1C8[index]`. Otherwise selector at
`C71C+4*index` picks a source pointer from `C1C8+4*selector`; the same state
index is read from that selected array. Slot0 is freshly replaced by the
current material state array. Source pointers/selectors are caller input,
not guessed defaults or a guessed maximum native array extent.

Source `ApplyMaterialStateSetForAnalysis` preserves forced lighting, index
ordering and per-state overrides, using common restored state translation,
lighting and lower device cache. Native failed HRESULT remains ignored.
Invalid source slots/NULL/malformed table values are explicit host guards,
not claimed safe native branches. No native error branch was faked.

## Evidence

`pc-material-install`: **6/6 exact captures,81 native assertions**. Install
none/color/shared; batch default/overrides/failed device, repeated and changed
inputs. Captures include raw17 colors before/after, all owners' stamps and
colors, clocks, intermediate raw states/color block at every COM event.
Max1638 instructions/63,488 arena; all actual allocations freed. Initial
scout cleanup assertion checked too early, before destroying AnimManager;
fixture order corrected, no cap or internal behavior bypass.

Build51+2, source45/45, full CTest**40/40**,37.59s. Run record
`local-data/results/bounded-native-runs/20260907T005514353112Z-pc-material-install.json`.
Bounds unchanged100k/2s call,30s child,64KiB arena,32KiB native allocation.

PC DXRenderer55→57, DXMaterial60 retained with integration evidence. PS2
unchanged. Actual full render entry4BC4A0 statically links installation,
state batch, COM material, pass update45F570 and per-pass draw4BC290;
the complete path is not yet executed/proven here. Full startup/reset,
nonstandard shader/layer paths, whole-file save/display remain open.
