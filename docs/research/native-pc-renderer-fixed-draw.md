# PC automatic no-weight draw — CP40

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual whole4BC290 now executes4BE310→4C8980→4BE2B0 with NULL selected
vertex shader and component flags0x801 (no blend-weight bits). The manager
is created by its original factory, returnsNULL through its real low-nibble
branch, and4BE2B0 returns without touching constants because selectionNULL.
Native assertions require all three calls and exclude generating4C89EB and
descriptor evaluation4AE930. No internal method is replaced by a seam.

An initially non-NULL bound vertex identity is cleared before COM+170(NULL).
Subsequent draws suppress repeated NULL binding. Primitive argument mapping,
pixel enable/binding and ignored HRESULT behavior use the same original
command path as [CP37](native-pc-renderer-draw.md). Optional pixel object
remains explicit declared device input, not a compiled shader claim.

Source `DrawWithoutBlendWeightsForAnalysis` uses the existing key builder,
actual reconstructed PC manager and a shared private resolved-draw core;
`DrawPreselectedForAnalysis` keeps its original non-NULL branch guard.
No separate alternate renderer is introduced. Material getter must be
initialized; manager injection is an explicit portable lifetime boundary,
not original process-global startup. Weighted automatic generation still
refuses this entry. Later [CP42](native-pc-renderer-submit.md) and
[CP48](native-pc-renderer-weighted-submit.md) verify full unlit buffer/pass
submission, including a cached weighted path; generated misses and actual
display remain open.

`pc-renderer-fixed-draw`: **3/3 exact captures,21 counted native checks**,
normal/error/pixel branches. Prior `pc-renderer-draw` rechecked **5/5 exact,
39 counted native** after common-core refactor. Source56/56, build57/57,
CTest44/44 in34.62s. Initial build required a missing spMaterial forward
declaration; corrected before successful build/profiles. No native cap hit.

Reports:
`local-data/results/bounded-native-runs/20260907T021935698297Z-pc-renderer-fixed-draw.json`
and `local-data/results/bounded-native-runs/20260907T021944991785Z-pc-renderer-draw.json`.
Max6911 instructions/63,200 arena;100k/2s call,30s child,64KiB arena and
32KiB native allocation limits unchanged. No OS/GPU/game calls or asset writes.
PC Renderer66→67, PS2 unchanged, no class/gate completed.
