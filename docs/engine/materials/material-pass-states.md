# PC complete pass texture-state batch

## Prepare all eight texture stages

If `E454[E474]` is nonzero, compare each stage's mapped UV caches first:
nonzeroE954 invalidates raw state8 toFFFFFFFF; E920 differing from stage
invalidates raw state7. Then selected materialC18C/pass[argument] supplies
its layers. Their MaterialTexture virtual getter supplies desired texture
pointerC198; changed identity sets byteC1B8. Layer slot7 copies nine raw
state words into desired blockC29C (stage stride24 hex).

Unused stages clear desired texture pointers and mark dirty if changed.
Their nine desired words come from default materialC9C0, pass0: layer0 for
stage0, **layer1 for every higher stage**, independent of active layer count.
Default textures themselves are not bound; their state words alone are used.

All dirty texture bindings execute first, in stage order0..7, through actual
4BB650; dirty flag is cleared afterward. Only then does the state batch
visit each stage0..7 and each state1..8. Raw state0 is never applied.
Thus this outer raw cache suppresses repeated states1..6 even though the
inner4BB1F0 mapping itself always submits them. The two layers of caching
must not be collapsed into one assumed policy.

## Overrides and final blend

Selector `C748+4*(9*stage+state)` chooses one complete72-word desired-state
block startingC29C. Source block0 is the freshly prepared pass; alternatives
are explicit caller-provided blocks. This lookup is used by4BBBA0 regardless
of material override byteC1C4. No unsupported selector extent is invented.

4BC410 first executes4BBBA0, then writes pass+10 final blend to **material
state7 itself**. If C1C4 set, selectorC738 instead selects the desired raw
blend from material-state source pointersC1C8; writing the material still
happens even with override. Only changed raw blend calls4B0AD0(index7).
Original HRESULTs are ignored throughout these wrappers.

## Reconstruction and proof scope

PC DXRenderer58→62. No PS2 transfer/full class/gate completion.
Next4BC290 shader constants/draw and shader selection, then enclosing
4BC4A0 buffer/material/pass submission. Full startup, reset, GPU resource
creation, nonstandard layers, whole-file save/display remain open.
