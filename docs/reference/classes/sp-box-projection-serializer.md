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
native RenderNode teardown, whole index behavior, PS2 and whole-class
completeness remain unqualified. Whole index explicitly refuses.

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
