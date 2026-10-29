# spPyramidProjectionSerializer: PC parameter reader

`spPyramidProjectionSerializer` (`0x22F43149`) physically extends
[ProjectionSerializer](sp-projection-serializer.md). Its target is
[PyramidProjection](sp-pyramid-projection.md), ID `0x1CCA7732`. The portable
[source](../../../Sparkplug/Code/Sparkplug/spPyramidProjectionSerializer.h)
uses the real shared Renderable, Projection and reference readers. Original
header and translation-unit paths remain unknown.

PC factory `43B240` creates a `14`-byte serializer with primary table
`6DF928` and secondary table `6DF91C` at `+10`. Reader `43B320` first
executes Projection reader `468820`, including its Renderable section,
then consumes its own section. All three sections have separate terminators.

Own field0 contains exactly25 bytes: center Vector3, size Vector3, then a raw
flip byte. After each successful read the reader calls receiver virtual slot
`58` again. The actual Pyramid accessor `4331B0` returns `this+9C`; stores go
to parameter offsets `4..C`, `10..18`, then byte `0`. Center is therefore
published before reading size, and size before reading flip. A later refusal
keeps the already completed assignments. Repeated field0 records apply in
encounter order; all other fields are skipped through the shared DataBlock
operation. Words `B8`, byte `CC`, cached sphere and other state are preserved.

The source reacquires a borrowed typed reference view after each read. This
view represents the original accessor's fields without claiming native
contiguous storage or an original declaration. Raw32 vector words and the
raw8 flip byte are transported without floating evaluation or bool
normalization. This operation admits signed zero, nonfinite encodings and
zero height as data; it does not qualify numerical sphere/bounds workers on
those inputs. Their separate bounded arithmetic contract still applies.

Inherited field5 binds a borrowed typed RenderNode through Projection's
common resolver. Reference ownership and Node lifetime remain external.
Valid bounded sections and already resolved typed FAT inputs are admitted;
strict extents/I/O/field limits, incompatible graphs and poisoned contexts
are analytical safeguards. Missing or malformed native inputs, arbitrary
native derived accessor overrides, graph startup/teardown, arbitrary index,
PS2 and whole-class completeness remain open. Ordinary index retains
ProjectionSerializer's explicit refusal. Concrete serializer Clone uses
the existing CloneManager mapping and literal Copy profile; it is separate
from whole reader execution.

The native secondary index slot reuses `43B230`, the same body as Box; it
delegates to Projection `4687D0` without consuming the own parameter getter.
The separate explicit warm-empty PC entry checks a typed Pyramid and uses
the shared [three-null-reference contract](sp-projection-serializer.md).
It invokes the actual common source error adapter and reference resolver in
Material, Fog, RenderNode order; error Handle's empty false result does not
turn a null reference into index failure. Cold globals, queued handlers,
nonnull recursion, other FAT profiles, root save and PS2 remain unqualified.

`IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis` is a separate explicit
entry for an exact portable Pyramid serializer and Pyramid target under the
shared [preindexed relationship profile](sp-projection-serializer.md).
It consumes no own parameter getter. At least one exact typed
DXMaterial/Fog/RenderNode is present in the supplied FAT at IDs7/9/11;
only those unwritten entries, current Common/Save lookup bindings and stable
warm-empty error context are admitted. Native actual duplicate FAT calls and
portable existing-entry early return are distinct implementations of this
bounded no-recursion effect. Host admission guards precede all effects and
exclude arbitrary derived receivers. The null-only endpoint, general Index
refusals, recursive graph/root-save/PS2 exclusions remain unchanged.

PC writer `43B500` is secondary slot0. It executes the real shared Projection
writer, then always emits own field0 with UInt8 size reservation25. It
reacquires virtual `58` before each center12, size12 and flip1 write, then
patches the field size and terminates the section. Raw values and other object
state remain unchanged; defaults also produce the own record. The same real
reader consumes this output, including borrowed prewritten Node references.

The portable writer admits a stable live default Pyramid parameter binding,
null or externally retained already-written FAT DXMaterial/Fog/RenderNode.
Material/Fog readback uses explicit shared owners in the context and preserves
owning replacement/same-object behavior; RenderNode remains borrowed.
It invokes the existing nonconst virtual reference view through a read-only
`const_cast`; the original accessor's constness was not recovered. The default
accessor does not mutate the object, and the writer never writes through its
references. Arbitrary mutating overrides, source graph changes and reentrant
streams are outside this profile. Wrong target/derived serializer or missing
supported relationship context refuses before output. I/O failure requires
discarding the partial output; native failure parity, new inline graphs,
whole save envelopes and numerical workers are not implied.


The separate IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis endpoint
admits the exact Pyramid serializer/target under Projection's Fog-only
first-insertion/repeat profile: one typed Fog binding/registration, null
Material/Node, empty FAT/next1 or the same unwritten Fog entry ID1/next2.
Exact host type guards precede effects and call no GetTarget override.
Shared reference/FAT/Fog own algorithms remain unchanged. Native allocation
roles and full repeat byte guards are distinct from portable metadata and
ownership observations. Other insertion graphs, cleanup, failure parity,
root save, PS2 and whole-class completeness remain outside this profile.


IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis is a separate
explicit profile for one final DXMaterial with NULL physical name, zero passes
and NULL color controller; Fog and Node stay NULL. Its own Host context borrows
one exact MaterialSerializer binding, warm-empty PCErrorManager, nonalias raw
word and current Common/Save policy2 manager with exactly one registration.
FAT is empty/next1 or exactly the same unwritten Material ID1/next2, file0,
NULL name, zero offset/size and matching ID/object maps. All inputs stay live
and stable synchronously; exact type/shape guards precede effects and invoke
no GetTarget callback. Power at native B8 is not read or invented by Index.

An unregistered stack-local ForAnalysis adapter is an explicit source dispatch
substitution. The real typed binding is checked, but shared IndexResource uses
the local adapter's own callback after its unchanged insertion. That callback
qualified-calls the unchanged generic Material tail, pure only for the admitted
empty shape, then executes the additional warm-empty NULL-reference gate.
The declared substitution is pure empty tail followed by nested gate. Native
runs that gate inside actual Material own Index before its NULL-reference
completion; effect order remains post-insertion and before outer NULL Fog/Node.
Repeat's shared existing-entry return skips own dispatch and nested gate. The
adapter is never registered/retained; no mutable current context/global/TLS is
introduced. Its receiver/RTTI/factory/callback counts are not native mappings.

Native first insertion has four error gates, repeat three. Native entry36,
RB24 twice and ordered-link12 are distinct from portable containers/ownership.
Old null/preindexed/Fog-only endpoints and shared Material/Serializer/Find/FAT
algorithms are unchanged. Populated Material/controllers, protected Node walker,
recursive graphs/root save, allocation/failure/rollback/cleanup parity, cold
startup, PS2, hardware and whole-class completion remain outside this profile.
