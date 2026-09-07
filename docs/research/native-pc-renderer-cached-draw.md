# PC cached automatic weighted draw — CP45

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Full original4BC290 enters actual4BE310→4C8980 with absent selected VS,
hits an actual manager map populated by original4C87A0, installs the returned
real4C9F10 shader, calls4BE2B0→4AE930→4BE210 and submits constants/draw.
No compiler miss or internal engine algorithm is replaced with success.

Fixture key(0x11,0), flags0x803, material power0, raw color0, no lights/UV
transforms. Actual shader owns two allocated44-byte descriptors: type5
start0/count3 and type8 start3/count1; scalar+34 is4 rows. Four calls vary
bone/diffuse inputs and final primitive kind. Native COM handles NULL and
device calls observed only; no live shader generation or display claim.

## Exact cache behavior

- 4BE2B0 uses4096 bytes of uninitialized stack, evaluates descriptors and
  copies shader+34 rows through renderer primary+58 /4BE210.
- 4BE210 copies rows toCBC4+16*start and setsE444=max(previous,start+count).
  Old high-water mark/tail persist when fewer rows are copied.
- Changed selected identity becomes boundE44C before COM+170; constants
  COM+178 use0,CBC4,E444. Device HRESULTs ignored.
- Epilogue4BC3EC..4BC403 clears the automatically selectedE454[current]
  after drawing, but retains boundE44C. Repeated auto draws rebuild constants.
- Explicit preselection bypasses key/constant rebuilding and survives the
  epilogue; changed input matrices/material do not update its old constants.

Source `DrawCachedAutomaticForAnalysis` composes the shared manager/key,
shader constant evaluator and resolved-draw core. The manager is injected,
not an invented process-global startup. Cache miss returns incomplete.
`BuildFullyWrittenConstantsForAnalysis` separately guards every submitted
word in the4096-byte scratch. Scalar-only writes, UV fourth lanes and empty
descriptors cannot manufacture zero values for untouched native stack bytes.
The guard is an explicit host restriction, not original validation.

## Verification and limits

3/3 exact captures,15 native checks; source17/17, build70/70,
CTest48/48 in53.73s. Previous preselected5/5 exact39 native and automatic
no-weight3/3 exact21 native rechecked. Main report:
`local-data/results/bounded-native-runs/20260907T032552955186Z-pc-renderer-cached-draw.json`.
Regression reports032718836750Z-pc-renderer-draw and032741912666Z-pc-renderer-fixed-draw.
At most4327 native instructions/63424 arena; unchanged100k/2s per call,
30s child,64KiB arena,32KiB allocation. No OS/GPU forwarding or asset writes.
Initial scout incorrectly expected persistent automatic selection; original
epilogue disproved that expectation. Fixture assertion corrected, no native cap.

Generating miss, descriptor producer, full weighted mesh/pass composition,
startup/reset and loaded-scene display remain open. Matrix invalidation is
the next connected consumer dependency. PC DXRenderer72→74, no PS2 credit
or class/gate closure.
