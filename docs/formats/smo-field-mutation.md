# SMO field mutation subsystem

## Boundary

`SmoViewer.Core` owns the format-level mutation subsystem. It has two layers:

1. `SmoRawObject`, `SmoObjectFieldReader`, `SmoDataBlockWriter`, and
   `SmoMutationTransaction` expose lossless direct fields. Research tools may
   set, add, or remove any serializer field number from 0 through 255.
2. `SmoSchemaRegistry`, `SmoObjectCapabilities`, and `SmoPropertyMutation`
   assign semantics only to confirmed class/field combinations.

SmoLVLcreator uses the second layer. It does not expose arbitrary field numbers
or unknown payload bytes in its normal UI. Its editable scope is placement
position, rotation, scale, and collision entities. This keeps format research
power in the reusable core without turning the level editor into a hex editor.

## Mutation invariant

All schema-backed property writes use `SmoMutationTransaction`. The transaction:

- addresses a direct field by `(field type, occurrence, optional payload size)`;
- preserves opaque payloads which were not selected;
- promotes compact field-size headers when a payload crosses a size boundary;
- updates every enclosing field payload size;
- updates object-directory logical offsets and serialized sizes;
- updates confirmed inline object ID/size prefixes;
- rebuilds the FFPS container and parses it again;
- rejects a result which changes object identity or introduces structural
  parser errors.

A size-changing replacement of a field containing catalogued inline objects is
not guessed. Such an operation needs a branch mutation API because replacing
opaque bytes does not describe where the child objects moved. Same-size raw
replacement and arbitrary direct-field insertion remain supported.

The project layer now supplies that branch API without mutating imported bytes.
It can remove several inline branches as one reachability set, relocate a shared
leaf to a surviving consumer, or redirect one inline resource to a same-type
resource owned by another project asset. Redirect compilation changes every
reference-only field with the old stable ID, replaces the old owner's inline
field with a reference, removes the old directory interval, and recalculates
all enclosing sizes and offsets. The same redirect is applied to imported
objects, added asset blobs and generated reference-placement shells.

## Confirmed property schemas used by the editor

| Class | Property | Serialized field |
|---|---|---|
| `spNode`, `spRenderNode`, `spModel` | position | field 0, 12-byte `Vector3` |
| same | rotation | optional field 1, 16-byte `Quaternion` |
| same | scale | optional field 2, 12-byte `Vector3` |
| `spStaticRenderObject` | world matrix | field 1, 64-byte `Matrix4x4` |
| same | inverse world matrix | field 2, 64-byte `Matrix4x4` |
| `spCollisionInfo` | position/rotation/scale | slices of field 2, 40 bytes total |
| `spMeshBV` | collision geometry | field 0, structured variable payload |

Identity rotation and unit scale may be omitted by Sparkplug. When editing
requires them, the schema materializes `rotation` immediately after `position`
and `scale` immediately after `rotation`. This is a class rule, not an
object-name exception.

## Collision ownership

Collisions are serialized as their own `spCollisionInfo` branches with an
`spMeshBV` child. They are editor entities independent from visual placements.
SmoLVLcreator may create a non-destructive geometric association between a
visual entity and a collision entity so the user can move them together, but
that association is not written as a fabricated engine field.

The current save path bakes an edited collision delta into the `spMeshBV`
vertices and leaves the independently serialized `spCollisionInfo` transform
unchanged. The schema still describes that transform so future tools can edit
it through the same property transaction after its runtime behavior is fully
verified.

## Verification

The format test corpus currently covers 59 level SMO files, 151,211 directory
objects, and every direct field stream. Synthetic tests additionally cross the
UInt8/UInt16 payload-size boundary, add and remove an extended field type, grow
a nested object, update its inline size prefix, and materialize missing node
rotation/scale. The Alfea02_old level-editor regression covers the complete
save/reopen path, including the node-owned `vase09` case.
