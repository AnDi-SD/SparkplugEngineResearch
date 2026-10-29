# spBoxProjectionSerializer: PC geometry reader

`spBoxProjectionSerializer` (`0x5730539D`) physically extends
[ProjectionSerializer](sp-projection-serializer.md). Its target is
[BoxProjection](sp-box-projection.md), ID `0x32BB2F56`. The portable
[source](../../../Sparkplug/Code/Sparkplug/spBoxProjectionSerializer.h) uses
the same shared Renderable, Projection and reference readers. Original
header and translation-unit paths remain unknown.

PC factory `0x0043AC40` creates a `0x14`-byte serializer with primary table
`0x006DF76C` and secondary table `0x006DF760` at `+0x10`. Reader `0x0043AD20`
first executes Projection's `0x00468820`, including the Renderable section,
then reads its own section. Each section has its own terminator.

Own field0 is a pair of three-component vectors: center at `9C..A4`, followed
by size at `A8..B0`. A successful first vector read publishes all three
center words before the second read starts. Later failure does not roll back
that assignment. Repeated field0 values replace geometry in encounter order;
unknown fields use the common DataBlock skip operation. Other Box fields,
cached sphere and the unassigned `B4` word are preserved.

The reader transports raw 32-bit values, including signed zero and nonfinite
encodings, without evaluating them. This raw storage contract does not qualify
Box's numerical sphere/bounds workers outside their documented finite integer
profile. The fixture setter's numerical admission rule does not constrain
this original serialized-data producer.

Inherited field5 binds a borrowed typed RenderNode through the shared
Projection reader. Clear, replacement and destruction retain the external
owner's node lifetime. Copy is the inherited literal Base operation; Clone
creates the concrete serializer and registers the shared CloneManager pair.

The portable reader admits valid bounded sections and already resolved typed
FAT inputs. Section sizes, field counts and incompatible graphs are checked
by analytical guards. Malformed native I/O, startup, whole resource loading,
native RenderNode teardown, arbitrary relationship index, PS2 and whole-class
completeness remain unqualified. Ordinary index entry points still refuse.

PC index `43B230` delegates to Projection index `4687D0` and adds no own
relationships. Its separate explicit warm-empty PC entry uses the same
[null-reference contract and caller context](sp-projection-serializer.md):
three real shared empty-error gates precede three shared null-reference calls.
The concrete Box target is checked before effects. Null Material/Fog/Node,
stable supplied current empty PCErrorManager and initial empty save FAT are
required; cold acquisition, nonnull recursion and whole save are excluded.

The separate `IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis` entry
admits an exact portable Box serializer and Box target under Projection's
[supplied preindexed relationship profile](sp-projection-serializer.md).
At least one exact DXMaterial/Fog/RenderNode is already indexed at the supplied
IDs7/9/11, with only those unwritten FAT entries and exact typed serializer
bindings. Native duplicate FAT indexing returns false internally and the
generic wrapper succeeds without relationship recursion; the shared portable
core's existing-entry early return has the same bounded effect. Host type
and FAT guards precede effects. This leaves the null-only endpoint and general
Index refusals unchanged and qualifies no new graph, cold startup or root save.

PC writer `0x0043AF50` executes the shared Projection writer and always emits
own field0, including constructor geometry. It reserves a UInt8 payload size,
writes center and size in separate twelve-byte operations, patches size24 and
terminates the own section. Raw encodings are transported without evaluation.
The admitted domain matches Projection's stable null or already written,
externally retained DXMaterial/Fog/RenderNode profile. Material/Fog readback
uses explicit shared owners, including replacement and same-object reuse;
RenderNode remains borrowed. Exact bytes,
successful write/seek order and original/shared readback qualify these sections;
whole save envelopes, new inline resources and native I/O failures remain open.


The separate IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis endpoint
admits the exact Box serializer/target under Projection's Fog-only
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
