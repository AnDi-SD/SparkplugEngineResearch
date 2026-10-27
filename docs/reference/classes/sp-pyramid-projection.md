# spPyramidProjection

The PC `spPyramidProjection` (`1CCA7732`) physically extends
[spProjection](sp-projection.md), through Renderable and NamedObject. Its
factory is `41A1B0`, table `6DE868`, and native extent `D0` bytes. The portable
class has its own layout. Original C++ declaration paths and method spellings
remain inferred.

The shared Projection constructor establishes the common prefix. The concrete
constructor sets byte `9C` zero, three center words `A0..A8` zero, size words
`AC..B4` to floating one, word `B8` zero and byte `CC` one. Other concrete
padding/storage bytes are unassigned. Source construction requires captured
common Debug field `28` data; it does not reproduce the native Debug global
sequence. Copy is inherited Renderable Copy, preserving the destination's own
Projection and Pyramid state. Clone constructs fresh own state, registers its
pair, then invokes that Copy.

The shared sphere getter calls the concrete worker and returns cache `70`.
For width `w=AC`, height `h=B0` and depth `d=B4`, the worker computes
`r=h/2+(w*w+d*d)/(8*h)` and writes sphere center `(0,h-r,0)` and radius `r`.
It does not add center fields `A0..A8`. Any nonzero raw byte `9C` negates the
three sphere center components, including zero signs; it preserves the radius
and raw flag byte. Negative height can therefore produce a negative radius.

The bounds getter independently writes `center-size/2` and `center+size/2`
using `A0..A8` and `AC..B4`. It neither updates sphere cache nor uses byte `9C`.
Negative dimensions preserve unsorted bounds. The native X half-product stays
wide while Y/Z half-products spill to float before the center addition.

The admitted sphere/bounds data uses finite integer center/width/depth values in
`[-1024,1024]`, including signed zero, and nonzero signed power-of-two height
with magnitude at most `1024`. In that domain the sphere numerator is below
`2^23` and its denominator is a power of two, so these intermediate results
are exact. The fixture setter refuses other inputs before changing state.
This is an analytical boundary, without an arbitrary floating-point or
zero-height execution claim.

The qualified lifetime uses an empty opaque Projection container and null
bound-node data. The inherited PC FX setup directly uses the real shared
owned helper operation in its no-material/initialized domains. Node/Scene graph
production, populated records, material rendering, other extended workers,
native failure/global startup, PS2 and whole-class completeness remain open.

The native parameter accessor `4331B0`, virtual slot `58`, returns `this+9C`.
This is a known additional entry after the common Projection slots. The
[Pyramid serializer](sp-pyramid-projection-serializer.md) reacquires that
pointer after each successful center, size and flip-byte read. The portable
`GetSerializedFieldsForAnalysis` returns borrowed typed references to those
three fields; it does not reproduce their native contiguous layout. Serialized
raw-bit transport is independent of the bounded numerical sphere/bounds
profile above. Inherited typed bound-node reference binding is described by
[ProjectionSerializer](sp-projection-serializer.md); it does not produce a
Node/Scene graph or qualify its teardown.
