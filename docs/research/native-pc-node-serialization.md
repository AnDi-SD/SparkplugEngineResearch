# PC Node serialization: fields → relationships → owned tree

2026-09-06, checkpoint 9 of the continuing PC SMO/SAN goal. This is not 100%
Node, whole native Save, or a completed native editor. PS2 was not exercised.
Original PC image SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Original execution and exact comparisons

| Probe | Checks | Confirmed boundary |
|---|---:|---|
| `probe_pc_node_serializer.py`, 7 cases | 51 | Actual factory `4638F0` allocates **14**; Node factory `421E20` allocates B4; reader `463A70`, transforms, flags, repeats/skip, null target and failed position |
| `probe_pc_node_relationships.py`, 4 cases | 31 | Actual `4678B0` reference and `421A60` child attach; repeated/prebound child; stop before unchecked NULL collision attach |
| `probe_pc_node_writer.py`, 5 cases | 38 | Actual `463F10`, defaults, transforms, false Animated, epsilon boundary, first-write failure |
| `probe_pc_node_save_graph.py` | 10 | Actual `4639D0` indexing and `467350` nested write; two objects, repeated root, exact FAT extents |
| `probe_pc_node_billboard_fields.py`, 5 fixtures | 20 | Actual field6 setter; controlled stop before world callback, not full billboard reader proof |
| `probe_pc_node_corpus.py object.smo` | 12 | Actual FAT/file index then **outer `422940`**, unchanged real two-Node file, names/tree/local+world state and teardown |

162 checks include **24 assertions in deliberate stop-boundary fixtures**;
do not count them as completed reader executions. All use unchanged per-call
100k instructions/2 seconds, 64KiB bump arena, 32KiB max native request,
30-second child. Explicit stream/name/initialized RTTI containers remain
fixtures, not original global startup or real OS/GPU evidence.

`compare_pc_node_sections.py` compares five complete reader rows (identical
input, cursor, flags, 120 bytes of local/world float state), four complete
writer byte strings and the 57-byte nested graph. All ten cases match.
`compare_pc_node_corpus.py object.smo` matches two named DFS records, including
every float bit and child index, against the compiled source **whole loader**.
This does not upgrade the native staged outer into whole `422B50` evidence.

## Rules that cannot be replaced with convenient editor assumptions

- Position/scale set dirty state; quaternion converts without normalization.
  `(0,0,.5,.5)` becomes matrix `(.5,.5,0,-.5,.5,0,0,0,1)`.
- Field3 toggles Bone both ways. Fields4 and8 only set Static/Animated on
  nonzero input; a zero byte **does not clear them**. Fresh Node has Animated
  set even if field8 is absent or false. Native writer still emits field8
  false when the source flag is false: this is a real non-roundtrip case.
- Field6 values1/2 select bits100000/200000; any other UInt32 clears both.
  NULL camera yields identity in the independently established Node world
  math. Degenerate explicit camera is a safer host rejection, not native rule.
- Child is an owning edge. Duplicate same-parent attachment is a true no-op,
  not a second reference. First attach sets child flags5 and propagates active
  hierarchy; parent section final world update propagates position.
- Native collision field7 resolves then forwards even NULL into `421ED0`.
  The NULL fixture stops **before** the dereference; no fake successful return.
- Actual writer order is `0,1,2,3,4,8,5,6,7`. Mandatory field8 default bytes
  are `280100`, false `280000`. Transform epsilon bits are `3A83126F`.
  Existing PC matrix/quaternion helper is reused, not an invented codec.
- Root/child graph: IDs1/2, root offset8/size49, child offset31/size25 in a
  57-byte first reference. A repeated root appends ID1,size0 only.
- Child attach can lazily create separate SceneManager `45ADF0`, global75DB90,
  exact24. Fixture teardown explicitly destroys it; it is not Node-owned.

## Common reconstructed core

`spNodeSerializer` now implements bounded sections/read/write/index via the
same `spSerializer` registry, reference framing and FAT used for SAN/meshes.
Protected section helpers allow derived serializers to consume sequential
base/derived sections without a second import implementation. Until a derived
adapter exists, strict public target guards prohibit silently saving only its
Node base. Collision payloads explicitly fail, not disappear.

Read context uses canonical `shared_ptr` owners. Created objects share their
owner with child edges; prebound/cache objects require an explicit external
owner. No invented no-op deleter is used. Holding the root past context teardown
keeps its child alive; dropping it releases the represented tree. This is host
ownership, not a proof of every native intrusive rollback or scene lifetime.

Field envelope/finite checks, full terminator/exact end, 16MiB section guard,
65536 field guard, existing reference depth64/object4096 limits are explicit
host restrictions. Different-parent reparent is still rejected; native scene
registration/collisions, graph writer depth and unknown-field lossless storage
remain open. Partial mutation on later error is not promised transactional.

`SparkplugNodeSerializationTests` adds normal/scalar failures, graph/owner and
whole constructed FFPS checks. Billboard integration includes NULL identity,
valid camera and degenerate rejection. The corpus CLI accepts only tiny512byte
Node files; its display names are not a general-purpose JSON/UTF8 exporter.

## Real assets and two failed whole-call attempts — do not retry

Read-only `Media/Menus/object.smo`,157 bytes:
SHA256 `6044809A6448DC5F960DE7FF7609B156DC0BA8165C4B50CDD6DFE4596575D1C2`.
Origin90, data67; Scene Root offset0/size67, object offset23/size43.
Native outer completed in90437 instructions/5584 allocated bytes.

1. Constructed whole Node FFPS `probe_pc_node_full_loader.py` reached100k at
   **88FDE3**, SP3000DD70 in original `422B50`. Disabled in code. Its separately
   successful writer does not prove whole Load. No resume or larger cap.
2. Unchanged `Menus/gameover.smo`,395 bytes, SHA256
   `593DDE72EAFC36532B4976B5269EB53D0B3FAC217AB9B97472C6B8C1DBEBE2AA`,
   six Nodes, origin200/data195: staged outer with explicitly initialized
   empty SceneManager reached100k at **89A355**, SP3000DB48, cursor254,
   heap6176. Visits: outer1/cache3/header3/Node factory3/reader2/reference2,
   **attach0/register0/name-set0**. This localizes the stop during the third
   factory; its cause is not closed. Disabled in probe and comparator.
   Source whole load succeeded, but no successful native differential claimed.

These are preserved unknowns, not discarded evidence or excuses to bypass the
guard. Next connected frontier: RenderNode → Model/Renderable → material/fog/
mesh relationships; collision, native Save and backend remain required.
