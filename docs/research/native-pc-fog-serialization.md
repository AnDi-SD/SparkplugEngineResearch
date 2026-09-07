# PC Fog codec, Model relationship and lifetime

2026-09-06 checkpoint11; continuing PC SMO/SAN goal, not100%/render-ready.
Original PC hash: `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Original instruction evidence

- Actual Fog419E90 allocation28 and serializer43B830 allocation14. Reader43B910,
  writer43BCF0: field0/type,ARGB,start,end,density,20 raw bytes; writer always
  emits UInt8-length headerA014 and terminator,23bytes.
- Default, values, repeats/unknown skip, enumFFFFFFFF, negative zero,
  qNaN7FC12345 and infinity7F800000 preserve exact bits. Nonfinite data is
  **not sent to renderer**; safe rendering is not inferred from a codec test.
- Unchanged703byte `Menus/logo_screen.smo`, SHA256
  `DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7`:
  **FAT ID5** Fog offset235+origin181=header416, section424..447exclusive.
  Actual type0/ARGBFF000000/start0/end1000/density0 differs from constructor
  end1/density1. Native read/write preserves these23 bytes exactly.
- Original read mutates **after each word**. Failed end after12bytes keeps
  changed type/color/start. Missing first word leaves defaults. First write
  failure produces zero output and diagnostics, not success.

`probe_pc_fog_serializer.py`:8 cases/60checks.
`probe_pc_fog_links.py`:4 cases/33checks. Actual Model4938F0 →
Renderable47FBA0 → reference4678B0 → Fog factory/reader → SetFog423D20,
then recursive index and Model/Renderable/Fog writers. Inline, repeat, clear,
prebound all exercised without a reference-result seam. Repeat keeps one
owned edge/refcount1; prebound keeps defaults; clear omits Fog from save.

## Native stale pointer and blank clone

Clearing the sole owned Fog deletes it, while native FAT entry.object still
contains its freed address. This is observed via allocation/lifetime records
and the FAT entry. **No dereference/reload of the stale address executed.**
Host context deliberately pins canonical shared owners until teardown; it does
not treat the freed pointer as a valid owning reference.

`probe_pc_fog_lifecycle.py`:6checks. Actual clone41A8E0/map registration412F70
returns a **blank default Fog**, not a payload copy. Inherited copy succeeds
without transferring Fog fields. Actual map initializer52FD90(global755588)
and teardown6D7DB0 execute, plus separate CloneManager24/global74E060.
First cleanup scout omitted that singleton; normal destruction fixes the
fixture audit. No native leak claimed, no allocation freed by guesswork.

## Source, tests and remaining boundary

Concrete `spFogSerializer` read/write/index uses common core and analytical
SectionCursor. Exact20byte envelope validation rejects truncation **before
mutation**, deliberately stricter than native per-word updates. Raw scalar
bits/enum are not clamped. Unknowns skip, not yet lossless.

`compare_pc_fog_sections.py`:9 exact rows (5scalar including real SMO slice,
4graphs), identical input/state/output. Native malformed cursor/partial-state
equivalence is explicitly not claimed. SceneSerializationTests230→381 (+151);
all30 CTests20.80sec. Existing source tests also check blank clone.
Native total **99checks across13 cases**;12-case profile plus standalone clone
pass. Workbench10/10 and diff check pass. No cap increase/new cap/game/GPU/asset
write. Limits100k instructions/2sec/64KiB arena/32KiB maxrequest/30sec child.
Max link reader17108instructions/5216heap, writer13333, scalar writer7531,
clone8338. Stream/name/diagnostic/startup remain explicit fixtures.

Checkpoint12 resolves physical word10 as an initialized NamedObject name.
`probe_pc_fog_named_prefix.py`5checks proves actual named-copy413120 retains
the external name entry and destructor uses413090. Engine RTTI still directly
names BaseObject. The new `named` graph case9checks proves common4671F0
**skips FAT-name assignment** due the engine RTTI guard. First opposite test
expectation failed in BOTH original and source and was corrected, not worked
around. Named graph output also matches exactly; Scene suite now401 checks.

Remaining: reentry/allocation/error/cache-reuse/cleanup variants, original
paths/API names/native NameManager lifetime, unknown lossless and complete scene/backend.
Nonempty Material/mesh, Texture and Skin are next neighboring dependencies.
