# PC material UV → renderer matrix submission (CP23)

2026-09-06; pristine PC
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Connected follow-up to [CP22](native-pc-uv-functions.md), not a live display test.

## Original call sequence / corrected interface

MaterialTexture467B70 reads UV38. If nonnull, compares accumulated20/applied1C;
unequal/unordered invokes controller slot20 (UV434820). If UV exists it always
submits this holder's matrix3C. Without UV, submission occurs only when static60
is set. Renderer global75DB68 secondary interface18 uses byte60 = **slot24**.
After submission, AnimTex64 is updated unconditionally if present, even when
its clocks are equal. Return values from controller/backend are not checked.

Actual PC interface6F28A0 (DX6EF9C8 also) has:

| Slot | Entry | Input | Device submission |
|---|---|---|---|
|23|4BB590|4×4|copy64, SetTransform(stage+16,matrix)|
|24|4BB4B0|3×3|embed top-left3×3 in4×4, last diagonal1, same call|

Cache entry is renderer base `F0F4+64*stage`; device is baseC9E8.
Both cache before submission, do NOT compare against previous cache, ignore
device HRESULT and return AL1. The actual device virtual offset isB0.
462680 implements the3×3 expansion;41D330 copies64. This expansion differs
from UV's intermediate affine4×4 layout and must not be interchanged with it.

Old renderer dossier incorrectly treated PC23/PS2 23 as signature-equivalent.
PS2's independently documented material boundary uses3×3 at callable23;
PC uses24 for that shape. New analytical SetUVTransform3x3 mapping is PC24,
PS2 23, with no new PS2 research credit. Legacy PC4×4 entry23 is preserved.

## Directed original/source comparison

Eleven cases cover no transform, static(success/failure), UV idle/active/shared,
UV+AnimTex, AnimTex without UV, direct3×3, direct4×4(success/failure).
Each runs three calls at deltas1,0,.5 and alternates stages0/1/0. Actual original
matrix/cache/device payloads match portable reconstruction exactly.

Shared UV updates the last-bound holder while the first holder submits its
own unchanged matrix. Equal clocks suppress UV computation but not submission.
Combined fixture uses one finite-duration NULL texture key: SetTransform sees
AnimTex phases0,1,1; after calls they are1,1,.5. It proves UV→submission→AnimTex
order without loading textures or increasing the arena. Anim-only sends no UV
matrix but still updates on all three calls.

Source methods live in the existing spMaterialTexture and spDXRenderer classes.
DX methods take an explicitly supplied cache entry; unproven full cache extent,
startup and device owner are not invented. Missing callback/unsafe evaluator
is a labelled host refusal; no device HRESULT error propagation is fabricated.

`pc-uv-renderer`: **11/11** fresh bounded children, **87 native assertions**,
11 exact captures. Source UV suite **386/386**; connected build58/58 and final
test build2/2; full **CTest35/35,2.71s** (preceding run26.25s).
Peak2,200 original instructions per update; arena≤63,040 bytes. This includes
explicit renderer backing F174 bytes for exactly two cache slots, NOT a native
large allocation or constructor. Native allocation limit32KiB and overall
arena64KiB,100k instructions/2s call,30s child remain unchanged.
Only external COM SetTransform is observed; material/UV/scalar/renderer
algorithms run original instructions. All native tracked allocations released.

## Remaining / safety

No full renderer initialization, render-pass/shader/texture binding/device-reset
or GPU output is proven here. Stage extent/invalid stage behavior, exceptional
math, resource reload and whole-file writer/display remain open.

A separate next-frontier MaterialColorController factory41A580 scout hit the
instruction/time cap at88CD04. That mode is permanently disabled before entry;
no factory/whole-load credit and no retry/resumption. ColorFuncEval factory50
and finite interpolation are independent successful scouts, not yet a CP23
class assessment. Continue that scalar dependency and static material paths.
