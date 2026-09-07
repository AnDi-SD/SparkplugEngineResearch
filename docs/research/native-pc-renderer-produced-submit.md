# PC parameter producer → complete cached draw — CP50

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Integration witness only, **no additional percentage credit** beyond CP49.

Extends [CP48](native-pc-renderer-weighted-submit.md) by replacing manually
declared descriptors/count with original protected4AF940 parameter production.
Real factory shader receives by-value named BlendMatrices, MatDiffuse and
optional view_proj_matrix; original append resolves types, allocates vector
and sets row sum4/8. Then the same real cache-hit and whole4BC4A0 run.
Source uses the common `AppendParameterForAnalysis`, not a second descriptor
producer. Repeated draws update bone/material and optionally dirty matrices.

`pc-renderer-produced-submit`:2/2 exact10 native, source171/171 combined
submission assertions; incremental build2/2,CTest51/51 in6.41s. Report:
`local-data/results/bounded-native-runs/20260907T040617007035Z-pc-renderer-produced-submit.json`.
Draw calls at most15250instructions, arena64624; producer individually
covered by CP49 at unchanged caps. Factories/vector/code path actual, COM
device handles explicitly NULL. No compiler, full lit/textured scene, source
mesh adapter or live GPU claim; no original resource writes.
PC scores, PS2 scores and all completion gates unchanged.
