# PC material-state translation — CP30

Pristine EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
This is actual CPU state translation to observed COM calls, not GPU rendering
or a complete pass/mesh submission. PC only; no PS2 score transfer.

## Layer state output

Original protected423590 reaches49E4D0. Arguments `(stage, output)`;
stage is not used. It copies nine words from nested texture+10..30 to the
writable output in order and returnsAL1. No device/state application occurs
here. Defaults and arbitrary raw words match for stages0,1,2,FFFFFFFF.
Initial scout usedNULL output and faulted on49E4F1; corrected valid output
succeeds. This was an uncapped ABI/input fault, not a native hang/retry.

Source `spMaterialTextureLayer::CopyTextureStatesForAnalysis` copies the PC
nine-word prefix from shared portable storage, not all twelve PS2 words.
NULL nested texture returnsfalse as a host guard; original requires a valid
pointer. Partial overlapping output and malformed pointers are not claimed.

## Raw material state to device state

4B0AD0 complete renderer writes `C868+4*index=value` **before** dispatch.
Indices outside1..10 returnAL0 after that write. The isolated0/11 tests stay
within the established12-word common cache; arbitrary out-of-array writes are
not executed. The mapping calls real4B0A90 with device-state indices below.

| Raw index | Value | Device state/value |
|---:|---|---|
|1|any|8 = value==1 ? 2 : 3|
|2|0..1|9 = value+1|
|3|0..2|22 = value+1|
|4|any|7 = value==1|
|5|any|14 = value==1|
|6|0..7|23 = value+1|
|7|0,1,2,3,4,6|19/20 = (2,1),(9,1),(5,6),(1,4),(2,2),(5,2)|
|7|5 or >6|no device calls; still true and raw cache updated|
|8|lighting mode|virtual slot35/+8C →4BDB10; separate pending frontier|
|9|any|24 = raw value|
|10|0..7|25 = value+1|

Lookup tables6EFD0C/14/20/40 have no native range guard. Portable method
rejects out-of-table values instead of emulating out-of-bounds reads. It
explicitly rejects still-unsupported state8; do not interpret that host false
as the original lighting result. A lighting0 native scout succeeds but has
no completed source comparison/coverage claim in this checkpoint.

Original4B0A90 compares the separate device cache `E4F4+4*deviceIndex`, calls
COM+E4 if changed, then updates that entry, ignoring HRESULT. The material
wrapper ignores that result too. Thus observers see **new raw state, old
device cache**. Repeating an identical value suppresses device calls even
after a failed HRESULT. Mode7's two mapped states are independently cached.
No direct raw-cache equality gate suppresses the translation itself.

Source chains `ApplyMaterialRenderStateForAnalysis` into the existing
`ApplyRenderStateCacheEntryForAnalysis` through a declared source dispatcher;
native4B0A90 is NOT seamed. Source observer reads the actual cache reference
being mutated, not a precomputed expected value. Fixture's256 device-cache
entries are a declared bounded input subset, not native total extent/default.

## Verification and remaining work

Six exact captures: default/raw layer output, state map, failed device,
poisoned initial device cache, repeated values. **272 native assertions**,
**278 source tests**,build57/57+2/2,full CTest**39/39**,28.51s. Renderer backing
F174 and small real Std/texture fit arena62,560; maximum1645 instructions for
protected layer dispatch,65 for mapped-state call. All native allocations
freed. Bounds remain100k/2s per call,30s child,64KiB arena,32KiB request.

Run record `local-data/results/bounded-native-runs/20260907T002301584991Z-pc-material-state-map.json`.
Native/compare `pc_material_apply.py`, existing renderer/state tests and common
source reused. Successful comparisons suppress huge raw stdout captures;
direct native runs still print captures. This affects reporting only, not
tests, evidence scoring or native execution.

Scores: DXRenderer46→49; MaterialTextureLayer65→72; StdLayer65→70.
Still required: texture/sampler mapping4BB1F0, lighting4BDB10 and material
color source selection, actual full pass/mesh path, reinitialization/reset,
whole-file Save and real display. No class100 or completion gate passed.
