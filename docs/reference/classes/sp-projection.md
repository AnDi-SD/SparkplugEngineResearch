# spProjection: bounded PC construction and geometry interface

`spProjection` (`0x5CB4145D`) physically derives from `spRenderable`. The PC
Box constructor establishes the BaseObject, NamedObject, Renderable,
Projection and Box vptr stages in that order. Projection has no registry
factory. Its geometric workers are abstract; its Copy and null Clone are
inherited from Renderable. Header paths and analytical API names are inferred.

The [shared source](../../../Sparkplug/Code/Sparkplug/spProjection.h) preserves
the confirmed constructor state: three bytes `58..5A` are `1,0,0`, four words
`5C..68` contain float-one bits, word `6C` is `2`, sphere `70` is zero,
bound pointer `84` is null, record count `90` is zero, helper `94` is null and
word `98` is `1`. Unassigned storage is recorded separately in the
[PC layout](../../../Sparkplug/Analysis/PC/spProjectionAbi.h).

The native empty container has a 140-byte allocation with two self-links and
132 opaque bytes. The portable source uses an empty `std::list` with opaque
record storage. This qualifies empty construction and destruction; record
fields, nonempty producers and nonempty teardown remain unqualified. The
The PC ProjectionSerializer reader's field5 requests `spRenderNode` through
the common reference reader and stores its result at `84`, including null,
without retaining or releasing either pointer. The portable binding admits
an actual typed, externally retained RenderNode. The native common resolver
does not validate its expected-class argument; arbitrary resolved object types
and unstable lifetimes remain outside this profile.

The common Renderable constructor obtains raw field `28` data from a lazy
Debug helper. The portable constructor takes that captured datum explicitly;
the registry factory requires it to be bound first. This data substitution
does not reconstruct the global Debug sequence, startup or its allocation
effects. An unavailable binding is an analytical refusal.

Sphere access calls the derived sphere worker before returning cached sphere
`70`. Bounds access delegates directly to the derived bounds worker. Copy
transfers only the inherited Renderable fields and leaves all Projection
storage unchanged. The concrete Box clone constructs fresh own state before
that inherited copy.

`InitializePCForAnalysis` executes the shared
[owned-PCFX setup](../../../Sparkplug/Code/Sparkplug/spProjectionManager.h)
against the object's actual material relationship. It constructs and publishes
a real PCFX when needed, binds the owner and executes real FX initialization.
The admitted whole domain has no material or an already initialized FX byte.
Other results remain explicit prefix refusals. Empty-cache FX ownership uses
`unique_ptr`; the native primary/secondary pointer adjustment is not a portable
ABI claim.

Rendering, node/bound-data attachment, populated records, material-backed full
initialization, failure paths, PS2 methods and whole-class completeness remain
open. The portable declaration exposes the qualified operations only.
