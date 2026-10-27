# ProjectionFX: PC lifecycle and bounded initialization

`spProjectionFX` (`0xF147D2EE`) is the abstract CrossPlatform family;
`spPCProjectionFX` (`0xF37E1DC5`) is its PC implementation. The original PC
object has a primary FX interface and a secondary BaseObject interface at `+4`.
The concrete allocation occupies `0xA8` bytes. Its object-interface Clone returns
that secondary pointer. The portable [base](../../../Sparkplug/Code/Sparkplug/spProjectionFX.h)
and [PC class](../../../Sparkplug/Code/SparkplugPC/spPCProjectionFX.h) preserve
logical behavior and use the shared CrossPlatform, NamedObject and CloneManager;
they do not reproduce the native object ABI.

The constructor clears owner `+18`, initialized byte `+1C` and cache `+20`.
It leaves the later PC storage unassigned. The analytical
[layout record](../../../Sparkplug/Analysis/PC/spProjectionFxAbi.h) keeps those
unknown bytes separate from the portable implementation.

PC initialization reads the initialized byte first. Any nonzero value returns
success immediately, preserving its raw value and avoiding owner/material
reads. With byte zero, it reads the owner's material pointer `+20`. If that
pointer is null, it writes initialized byte `1` and returns success. An owner
bound by the projection setup operation is borrowed; initialization acquires
no reference to it. Repeated initialization reuses the same FX object.

The [host interface](../../../Sparkplug/Analysis/Host/spProjectionFxHost.h)
supplies only the borrowed material-field read. The source executes the FX
branch and byte write directly. Missing owners and material-backed
initialization return explicit analytical refusal statuses. These refusals are
not native false returns. This default entry refuses the material path before
its material and layer mutations. It does not invoke the separate prefix below.

`spPCProjectionFX::PrepareMaterialLayersPrefixForAnalysis` is an explicit PC
material preparation prefix. For an FX with zero initialized byte, its caller
supplies the same live typed material already bound at owner member `20`; this
does not reconstruct a Projection class or the graph producer. The
initialized-byte guard comes first, retaining
the whole early return for any nonzero raw byte. Otherwise the method requires
the established owner, writes material VertexAlpha byte `6D` to one, and reads
only the first pass. In that pass's existing layer order it reads each layer's
MaterialTexture and writes texture state indices `3=2`, `4=2`, `8=11`, `7=10`.
The other material passes and existing graph relationships stay unchanged.

The prefix stops before native `4C5060` calls the StdLayer factory. It returns
`UnqualifiedMaterialLayerCreation` after the represented writes, without
setting the FX initialized byte, creating a layer, or reporting a native
initialization return. Missing typed pass/layer/texture data returns
`UnqualifiedMaterialGraph` after completed prefix effects. Those statuses and
the missing-owner guard are analytical refusals. Actual material-backed
initialization, appended layers, textures, cache creation and rendering remain
open. Prefix qualification does not include resuming its stopped native guest
or tearing down the supplied material graph.

Concrete Clone creates a fresh FX and applies the inherited name-only Copy.
Owner, initialized byte, cache and later PC state retain constructor defaults.
Base FX has no registry factory and its object-interface Clone is null.
The qualified destructor domain has an empty cache. Material/cache allocation,
populated-cache teardown, the primary draw method, shader/driver work, PS2
behavior and whole-class completeness remain open.

Projection and global service lifetimes are separate. Native projection setup
and destruction demonstrate ownership of its FX, while the current portable
FX class implements its own lifecycle over explicit borrowed owner data. A
BoxProjection source implementation and global CloneManager/DebugManager
shutdown are not inferred from these paths.

The separate `InitializeProjectionPCWithOwnedFxForAnalysis` profile in the
[projection setup source](../../../Sparkplug/Code/Sparkplug/spProjectionManager.h)
also executes the PC projection setup sequence over an explicit owned FX
field. A null field acquires a real shared `spPCProjectionFX`, publishes it,
binds the borrowed projection owner and calls its real initialization. Existing
fields are reused and their owner is overwritten even when an initialized byte
causes an early return. The source field uses `unique_ptr`; this is portable
storage for the bounded operation, not the native Projection class layout.

The field, helper and borrowed projection must remain alive and stable during
the synchronous material read. A material refusal leaves the new helper and
owner binding in place, matching the observed native prefix before its first
material mutation. Such a refusal cannot be discarded as a successful native
return. The manager's original borrowed-helper profile remains distinct and
continues to refuse a missing helper before acquisition.

The explicit `EvaluateDrawGateForAnalysis` operation preserves the early PC
primary Draw gates. It consumes stable captured RenderNode fields: nullable
Scene stamp `40`, support visibility mark `118`, and Node flags `B0`. A missing
Scene or unequal stamps skips drawing and returns native success. Matching
stamps then test Enabled bit `0x200`; a clear bit also skips with success.
Matching zero stamps are valid values. These gates do not read FX owner or
initialized byte and do not consume the camera argument.

The input record is a data binding for existing foreign state, without a
Scene/Node producer or atomic snapshot claim. Native verification executes
actual RenderNode support method `424790`. Active input returns an explicit
analytical refusal before camera, matrix, model and renderer work. The gate
profile does not qualify active drawing or native RenderNode teardown.
