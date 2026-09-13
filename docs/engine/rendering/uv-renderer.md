# PC material UV → renderer matrix submission

## Original call sequence / corrected interface

MaterialTexture467B70 reads UV38. If nonnull, compares accumulated20/applied1C;
unequal/unordered invokes controller slot20 (UV434820). If UV exists it always
submits this holder's matrix3C. Without UV, submission occurs only when static60
is set. Renderer global75DB68 secondary interface18 uses byte60 = **slot24**.
After submission, AnimTex64 is updated unconditionally if present, even when
its clocks are equal. Return values from controller/backend are not checked.

Actual PC interface6F28A0 (DX6EF9C8 also) has:

| Slot | Entry | Input | Device submission |
| --- | --- | --- | --- |
| 23 | 4BB590 | 4×4 | copy64, SetTransform(stage+16,matrix) |
| 24 | 4BB4B0 | 3×3 | embed top-left3×3 in4×4, last diagonal1, same call |

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

Source methods live in the existing spMaterialTexture and spDXRenderer classes.
DX methods take an explicitly supplied cache entry; unproven full cache extent,
startup and device owner are not invented. Missing callback/unsafe evaluator
is a labelled host refusal; no device HRESULT error propagation is fabricated.

## Remaining / safety

No full renderer initialization, render-pass/shader/texture binding/device-reset
or GPU output is proven here. Stage extent/invalid stage behavior, exceptional
math, resource reload and whole-file writer/display remain open.
