# PC standard material graph and DX runtime identity

Checkpoint 13, 6 September 2026. Bounded original x86 observations, not a claim
of complete material/render support. Original executable SHA256 remains
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## File identity is not runtime identity

MaterialData serializer42F690 (primary6DE5C8) and DXData serializer42F3E0
(primary6DE520) both use header hook **42F4C0**. It reads eight bytes and,
after successful stream read, calls **4A9460**. Neither class ID nor SBOO is
checked/used there. Actual arbitrary-header probe creates **spDXMaterial**.
The common MaterialSerializer uses467550 instead; do not change its factory.

| Role | Original PC evidence |
|---|---|
| spDXMaterial | ID797B39EC, direct Material5C0314C5, record7630E8 |
| registration | initializer6D4C50, registration call6D4C70 |
| factory/layout | 4A9460, BC bytes, primary6EF264, secondary6EF238 at this+14 |
| serializer startup | 6D4C80 calls4B0DD0, registers runtime797B39EC, masksFF/3 |
| spDXMaterialSerializer | ID177E2F26, direct MaterialSerializer2A14745F, record763AE0 |
| serializer | factory4B0DD0,3C bytes, primary6EFD74, secondary6EFD68 |

DX serializer header uses common467550; secondary write/index/read are
4B0CF0/4766D0/42F670. Its primary layer helpers4B0EB0/4B12A0/4B0F10 are
different from the base: static code dispatches extra behavior for
**spDXShaderLayer71643E66**, not StdLayer. CP29 refinement: the exact shader
RTTI is rejected by the common base helper before that extra tail; see
[actual factory/lifetime and negative codec evidence](native-pc-shader-layer.md).
This passage never meant that a Std input gets converted into DXShaderLayer.

An initial graph fixture seeded MaterialData/Std RTTI only. Read succeeded,
but recursive indexing asked for runtime7630E8, whose ID was zero in that
fixture; NULL dispatch faulted at46733E. Corrected explicit runtime record and
the actual6D4C80 serializer registration. No class-zero registration, vtable
replacement or forced success. This was an uncapped incomplete-input fault.

## DX material defaults and scalar copy

Actual factory/dtor and header probes run without renderer/GPU. Physical
prefix agrees with MaterialData78, but DX ambient/emissive are **(0,0,0,0)**,
not MaterialData's opaque black. Diffuse/specular are(1,1,1,1).
**Power+B8 is untouched**: CC allocator poison is an observation of no write,
not a valid constructor default. Safe source tracks initialization and refuses
to serialize an unset power instead of inventing a native value.

Clone4A94C0 calls copy4A9570 -> Material423880, then copies17 words78..B8.
The scalar-only probe confirms colors/power, states and rawflags6C/6D copy;
Base auxiliary pointer+4 and runtime70 are not copied. Unlike MaterialData's
blank clone, changed payload survives. Complex pass/controller clone policy
is not closed; source rejects that unsupported DX clone shape. Original
header path and API names remain inferred/analytical.

The first scalar-copy fixture left a synthetic non-NULL auxiliary-pointer
sentinel in the source during destruction and faulted trying to follow it.
Cleanup now restores that external sentinel toNULL before the original dtor;
it was not a native lifetime bug or instruction cap.

## Standard-layer field grammar and ownership

Seven tiny inputs exercise common actual MaterialData read42F670->4774D0,
index4766D0 and write4B0CF0->476B50 with an explicit valid Std RTTI tree.
This is not a replay of the disabled whole-logo allocation-capped scout.

- field3 creates owning MaterialPassLayer38 and reads blend u32;
- field4 contains StdLayer234C576B; real factory460E50 creates outer14 and
  nested MaterialTexture68, not a flattened texture record;
- fields8(legacy)/17 contain nine u32 texture states. Actual defaults are
  `[0,3,1,0,0,0xFF000000,2,0,0]`; source comparison caught/fixed a one-byte
  shifted earlier default. Repeated fields overwrite in place;
- field9 is **u32 flag + nine floats =40 bytes**. Nonzero sets static flag
  to1 and copies matrix. Zero consumes the matrix but leaves prior flag and
  matrix unchanged. Writer emits flag1, not the original nonzero value;
- writer order for Std: field4,17,optional9. Native small state capture is
  36 state bytes +36 matrix bytes +one static byte. Default matrix identity.

Ownership is not uniformly intrusive. Material retains Pass(ref1), but Pass
directly deletes Layer and Layer directly deletes MaterialTexture(ref0,0).
45F5E0 replaces/deletes a layer, then sets count=max(count,index+1), **even
forNULL**. It does not shift/shrink like Material423960. 45F5B0 clears all
eight slots regardless of count. Source now uses unique owners for these two
direct-delete edges and shared canonical owners for the Material->Pass edge.
Same-pointer destructive replacement and out-of-bounds native calls not run.

An early UV input omitted the flag (36 bytes): native returned true with a
read diagnostic and consumed across field boundaries. Correct valid inputs
are40 bytes. Safe host rejects the malformed field before mutation; native
malformed acceptance is not presented as a safe parsing contract.

## Connected Model graph

Four modes: inline, repeated same reference, clear viaNULL, prebound. Real
Model4938F0 -> Renderable -> common4678B0 -> header/factory -> Material fields
loads the graph; actual recursive index4672C0 and common reference writer
write nested output. Inline/repeat create DXMaterial; prebound MaterialData
stays MaterialData because factory/payload are skipped. Pass/Layer inline
objects do not acquire independent FAT entries. Explicit Color field in the
valid graph initializes DX power; earlier scout output containing poison
is not used as a portable golden file.

Native clear deletes the sole-owned material but FAT retains its stale
address; never reused/dereferenced after deletion. Portable read context
keeps a canonical owner alive, an explicit safe lifetime difference.

## Remaining boundaries

This is the historical CP13 checkpoint. CP19–28 subsequently verify common
texture/controller links, finite evaluators, prebound material color, frame
stamps and selected device boundaries; see the current cycle journal and
platform ledger. Their factory/error/whole-file/display limits remain explicit.

Validation: **116 native assertions**, **11 exact source/native rows**;
`pc-material-standard-graph` **16/16** fresh bounded children, entire CTest
**31/31,21.64sec**, Material suite**367 checks**. Original assertion breakdown:
seven layers56 +direct-owner7 +DXfactories7 +header/copy15 +four Model graphs31.
Earlier scalar profile remains a separate regression, not new research credit.
Workbench unit10/10 and platform-ledger unit12/12 pass. `git diff --check`
passes. An initial unittest invocation used the wrong import search path;
direct repository test entrypoints pass without source changes.

Nonempty external textures, color/UV/animation controllers, all nonstandard
layers (including DXShaderLayer), material update/render application, full
clone/alias/reentrant/allocation failures, unknown-field lossless handling and
whole SMO Save are not complete. Existing capped probes remain disabled.
PC-only evidence; no PS2 credit, game/GPU execution or asset changes.
