# PC RenderNode → Renderable/Model serialization

## Independently executed entries

| Class | Exact allocation | Reader | Actual secondary writer |
| --- | ---: | --- | --- |
| spRenderNodeSerializer | 14 | 469190 → Node463A70 | 469340 → Node463F10 |
| spRenderableSerializer | 14 | 47FBA0 → target13E7FB0 | 47F7A0 → target1402630 |
| spModelSerializer | 14 | 4938F0 → Renderable47FBA0 | 4935F0 → target1404A30 |

## Common reconstructed implementation

Original-named serializers implement sections/read/write/index via existing
spSerializer/FAT/DataBlock and canonical context shared owners.
`Analysis/PC/spSectionCursor.h` is an **analytical host envelope guard**, not
an original class or second codec. Logical extents/field counts, exact field
consumption and final section terminator are checked.
Base guards prevent unimplemented derived serializers, notably Skin, from
silently writing only Model/Renderable sections. Nonempty typed Model mesh/
Полное поведение material/fog adapters здесь не описано.
Unknowns are skipped, **not preserved losslessly**. No transactional rollback.

## Failed scouts and remaining obligations

Remaining: nonempty mesh/material/fog and texture chains; Skin; shared lifetime/
errors/lossless unknowns; Node collision; whole native Save, real SMO scene and
PC backend. Earlier Node/visibility/whole-SAN caps remain disabled, not bypassed.
