# PC renderer matrix setters/raw getters — CP47

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original secondary interface6F28A0 (this=complete+18):

| Slot | Entry | Complete destination | COM+B0 transform ID | Raw getter |
|---:|---|---|---:|---|
| 12 | 4BBAE0 | CAC0 | 3 | 4AD370,slot18 |
| 13 | 4BBB20 | CA80 | 2 | 4AD360,slot17 |
| 14 | 4BBB60 | CA40 | 256 | 4AD350,slot16 |

Projection/view/world labels are analytical matrix roles; exact member/API
spellings are not surviving original names. Each setter calls actual41D330
to copy16 raw words, writes dirtyF2F4=1, then calls COM+B0 with the cached
matrix address. Always submitted, even identical repeated input or self-alias.
HRESULT ignored; nativeAL=1. World entry ret8 consumes an unused second
argument, not an implemented palette/index selector. Raw getters return
the input address and never trigger4AD540 or clear dirty.

Source `SetInputMatrixForAnalysis`/`GetInputMatrixForAnalysis` use the same
explicit carrier as [CP46](native-pc-renderer-matrices.md). Setters preserve
raw NaN words because native copy performs no arithmetic; later finite-only
matrix multiplication remains a separate explicit source guard. Missing
callback/out-of-range analytical index are host rejections, not native errors.

`pc-renderer-matrix-inputs`:5/5 exact captures,35 native checks;
world/view/projection, HRESULTfailure, self-alias;6calls per case.
Source31/31,build69/69,CTest50/50 in40.36s. Report:
`local-data/results/bounded-native-runs/20260907T034059846715Z-pc-renderer-matrix-inputs.json`.
At most55instructions/62816arena. Limits100k/2s,30s,64KiB/32KiB unchanged;
no GPU/OS forwarding. Full constructor, camera/skinning producers, reset,
matrix-stack caller behavior and loaded-scene display are not claimed.
PC DXRenderer75→76; no PS2 credit or completed class/gate.
