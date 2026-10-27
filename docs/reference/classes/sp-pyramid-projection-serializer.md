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
native derived accessor overrides, graph startup/teardown, whole index,
PS2 and whole-class completeness remain open. Whole index retains
ProjectionSerializer's explicit refusal. Concrete serializer Clone uses
the existing CloneManager mapping and literal Copy profile; it is separate
from whole reader execution.

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
