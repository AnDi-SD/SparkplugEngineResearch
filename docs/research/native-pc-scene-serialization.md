# PC RenderNode → Renderable/Model serialization

2026-09-06, checkpoint10 of the continuing PC SMO/SAN goal. Not100%, no
game/device execution or PS2 credit. Original PC image SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Independently executed entries

| Class | Exact allocation | Reader | Actual secondary writer |
|---|---:|---|---|
| spRenderNodeSerializer |14|469190 → Node463A70|469340 → Node463F10|
| spRenderableSerializer |14|47FBA0 → target13E7FB0|47F7A0 → target1402630|
| spModelSerializer |14|4938F0 → Renderable47FBA0|4935F0 → target1404A30|

Historical body labels47F7B0/493600 are **not ordinary callable writer entries**
in this protected image. Probes invoke actual secondary vtables with
`this=serializer+10`, `(stream,target)`. Actual factories469040/47F650/4934C0
and native destructors execute;14 is no longer only observed extent.

`probe_pc_scene_serializers.py`:12 scalar cases/76 checks.
`probe_pc_render_node_sections.py`:3 successful graph cases/30 checks plus
NULL **stop-before-diagnostics**4 checks. Total110 includes those4 stop
assertions, not110 complete reader executions.
`compare_pc_scene_sections.py`:15 exact rows (12 scalar+3 graph), identical
input, return/cursor/state/edges and every output byte on13 successful writes;
two scalar errors match false/no-write. Profile `pc-scene-sections`:16/16.
Source `SparkplugSceneSerializationTests`:230 checks; all30 CTests20.10sec,
workbench10/10 and diff check pass.

Max graph reader32384 instructions/57408 allocated bytes, writer15937.
Unchanged100k/2sec per call,64KiB bump arena,32KiB max native request,30sec child.
Renderer cache storageC9C8, RTTI startup records/tree, stream/name/diagnostic
boundaries are explicit fixtures, not native startup/Windows/D3D evidence.
No new capped call.

## Rules

- RenderNode consumes complete Node section including world callback, then its
  own repeated field0 Renderable references. Append469ED0 retains **every
  occurrence**: duplicate alias gives2 entries/refcount2, unlike Node child no-op.
- Inline Model uses actual FAT/registry/header/factory and inherited
  Renderable/Model readers. Prebound Model preserves defaults. These specific
  graph fixtures contain no mesh/material/fog.
- Renderable alpha field2 is **UInt32**, `!=0` normalized to byte18, then
  classifier slot34. Priority3 preserves UInt32 at1C. Repeats are last-wins.
  Both always written: default `6201000000630000000000`.
- Explicit NULL material/fog is accepted. Material also invokes classifier;
  PC Model's classifier override is the independently known no-op. Nonempty
  replacement lifetime is not proved by NULL cases.
- Missing Model mesh field is accepted. **Explicit NULL mesh** branches to
  diagnostic493A04, not a general end-of-object mandatory-mesh condition.
  Projection1 is UInt32/default3, always written with Begin/End length framing:
  `e1040000000300000000`, **not** minimal61. Reader493973 does not test Read
  result; strict source checks deliberately reject malformed group input.
- Each inherited section has its own terminator. Default RenderNode bytes
  `28010000`; default Model is Renderable bytes followed by projection section.
- Read publication fills entry.object, not the save by-object map. Separate
  actual recursive indexing is required before write. After clear/reindex,
  root/model IDs1/2; inline Model header+payload29 bytes. Repeat emits ID2,size0.

## Common reconstructed implementation

Original-named serializers implement sections/read/write/index via existing
spSerializer/FAT/DataBlock and canonical context shared owners.
`Analysis/PC/spSectionCursor.h` is an **analytical host envelope guard**, not
an original class or second codec. Logical extents/field counts, exact field
consumption and final section terminator are checked.
Base guards prevent unimplemented derived serializers, notably Skin, from
silently writing only Model/Renderable sections. Nonempty typed Model mesh/
Renderable material/fog adapters still require separate native/corpus validation.
Unknowns are skipped, **not preserved losslessly**. No transactional rollback.

Intermediate build caught a protected Camera constructor in a guard test;
the intended regression now explicitly passes RenderNode to the Node base
serializer. Shadowed variable warning corrected. Only the successful fresh
build and subsequent30/30 tests are credited.

## Failed scouts and remaining obligations

1. First graph writer omitted indexing and faulted4673AC on NULL FAT by-object
   lookup. Corrected by actual clear/index workflow, not a lookup seam.
2. Whole RenderNode NULL diagnostic hit external formatting
   `4169DF → 0033D03C`, a previously known unmapped boundary. Whole mode is
   disabled. Distinct `null-stop` stops at469288 **before** diagnostics,
   observes empty vector/block sentinel, aborts/cleans up without resuming.
   Complete native false-return path is not claimed.

Remaining: nonempty mesh/material/fog and texture chains; Skin; shared lifetime/
errors/lossless unknowns; Node collision; whole native Save, real SMO scene and
PC backend. Earlier Node/visibility/whole-SAN caps remain disabled, not bypassed.
