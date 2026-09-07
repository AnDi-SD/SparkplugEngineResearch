# PC material UV → TransFunctionEval → FunctionEval (CP22)

2026-09-06 timed PC-first cycle; pristine PC SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PC-only evidence. Analytical API names/paths are not recovered original names.

## Identity and storage

TransFunctionEval491432F0, registration761098/init6D4050, base87B0E260;
actual factory47D2E0 allocates1B0, vtable6EAAEC. Seven embedded FunctionEval38
objects:10/48/80 translation, B8/F0/128 scale,178 rotation. Pivot160 defaults0,
axis16C defaults(0,0,1); scale YOffset defaults1. Standalone clone47D340 uses
Base40ECE0 and returns factory defaults, unlike UV embedded assignment.

UVController1C0053D6, registration75D3C8/init6D1CF0, actual factory41A210,
size1FC/vtable6DE8CC. RenderController24 base, borrowed material24,
saved matrix28..4B, embedded TransFunction4C. Exact storage assertions are in
`Sparkplug/Analysis/PC/spUVFunctionAbi.h`, separate from portable host ABI.

## Runtime

PRS47CD10 evaluates **Tz,Ty,Tx,rotation,Sz,Sy,Sx**, important for shared RNG
ordering. All three validity bytes become1. Rotation is in **turns**: scalar
x87 return is multiplied by float32 2π at740370 BEFORE storing float32 angle.
Early rounding caused a random-rotation differential failure; a wide-result
entry in the same scalar evaluator fixed it without relaxing comparison.
464C80 yields axis*sin(angle/2),cos(angle/2), with no axis normalization/repair.

Matrix47CEE0 row-vector order is
`T(-pivot) * R * T(pivot) * T(position) * S`; final scale affects translation.
Shared4647F0 quaternion conversion and426B00 matrix multiply now preserve
observed intermediate stores/addition order. Host finite/double arithmetic is
not a claim of all80-bit x87, NaN, signed-zero sign or non-default rounding.

UV434820 consumes elapsed time4231A0, embeds saved3×3 as
`[a,b,0,0;d,e,0,0;0,0,1,0;g,h,0,1]`, multiplies by evaluated4×4, extracts
`[m0,m1,0,m4,m5,0,m12,m13,1]`. Saved projective entries2/5/8 are discarded.
Holder467CB0 stores36 bytes and sets static flag60. Holder update467B70 skips
UV recomputation when clocks match but submits even then if UV is attached.
PC3×3 submission is slot24/4BB4B0, not slot23/4BB590 (4×4); actual device
submission is the next connected checkpoint, not yet execution credit here.

## Ownership / clone

4346C0 restores a previous nonidentical holder's saved matrix, then snapshots
the new one. Alias binding only refreshes baseline. 467D90 retains/releases;
holder state30 becomes2(nonnull)/0(NULL), unless bit8 preserves the whole word.
Last-owner NULL destruction does NOT restore saved baseline. Shared UV updates
the last-bound holder, not necessarily the caller of material update.

UV clone41ABB0/copy434D60 copies clocks, saved matrix, embedded scalar runtime
state and pivot/axis. Material uses map-aware412C40. Unbound stays unbound;
pre-mapped uses supplied holder. Unmapped material recursively clones68 bytes;
its467DF0 copy uses **always-clone412BE0** for UV even with root UV mapped.
Thus a second UV clone is retained by the new holder, while returned root UV
has refcount0 and points to that holder. Both are torn down explicitly.
The412BE0/412C40 distinction was already established in BaseObject research.
Portable UV/Trans clone graphs remain explicitly refused: native observation
does not mean source clone parity. Host shared owners/detach avoid stale native
backlinks; those safety adaptations are separately labelled.

## Common codec / material integration

TransFunction nested field0 uses UInt8 reserved length, seven sequential scalar
sections, then24 bytes pivot/axis. UV field0 uses UInt32 reserved length around
the Trans section. Source codecs share existing SectionCursor/writer/reference
core. Scalar runtime time/clamp are not serialized. Raw codec IEEE states stay
representable; finite runtime guards are separate.

Material field12 resolves canonical UV through common ReadFieldReference and
ShareObject. NULL preserves previous, repeated ID aliases. Index/writer recurse
in native order after fallback/AnimTex. Four complete declared graph captures
(inline/repeat/NULL-after/two layers) match input, post-update matrices and
recursive output exactly. They are not whole SMO/display tests.

## Verification / open edges

- `pc-uv-functions`34/34 children,224 native assertions,34 exact captures
  (30 PRS/matrix/UV scenarios,4 codecs).
- `pc-uv-graph-lifetime`10/10,96 native assertions (44 graph+52 lifetime),
  four additional exact source/native captures. Total320 native,38 exact.
- Source UV348/348; final ABI/test build2/2 after connected82/82 and45/45;
  CTest35/35,29.12s. Scalar profile21/21 reverified after wide-return refinement.
- Graph peak51,423 instructions/7,328 arena bytes; random UV19,557 instructions.
  All actual tracked allocations freed. Limits unchanged100k instructions/2s,
  30s child,64KiB arena,32KiB native request. No OS/GPU forwarding or asset writes.
- Explicit streams, valid RTTI inputs and finite CRT floor are boundary
  contracts, not substituted engine algorithms; cold registration stays banned.

Malformed/partial/write/allocation failures, exceptional math, source clone
parity, complete setter/reset callers, whole-file save and display remain open.
No100%-class/global-gate claim; no new PS2 assessment.
