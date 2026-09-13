# PC material UV → TransFunctionEval → FunctionEval

## Identity and storage

TransFunctionEval491432F0, registration761098/init6D4050, base87B0E260;
actual factory47D2E0 allocates1B0, vtable6EAAEC. Seven embedded FunctionEval38
objects:10/48/80 translation, B8/F0/128 scale,178 rotation. Pivot160 defaults0,
axis16C defaults(0,0,1); scale YOffset defaults1. Standalone clone47D340 uses
Base40ECE0 and returns factory defaults, unlike UV embedded assignment.

## Runtime

PRS47CD10 evaluates **Tz,Ty,Tx,rotation,Sz,Sy,Sx**, important for shared RNG
ordering. All three validity bytes become1. Rotation is in **turns**: scalar
x87 return is multiplied by float32 2π at740370 BEFORE storing float32 angle.
Early rounding caused a random-rotation differential failure; a wide-result
entry in the same scalar evaluator fixed it without relaxing comparison.
464C80 yields axis*sin(angle/2),cos(angle/2), with no axis normalization/repair.

Matrix47CEE0 row-vector order is
`T(-pivot) * R * T(pivot) * T(position) * S`; final scale affects translation.
Shared4647F0 quaternion conversion and426B00 matrix multiply now preserve
observed intermediate stores/addition order. Host finite/double arithmetic is
not a claim of all80-bit x87, NaN, signed-zero sign or non-default rounding.

## Ownership / clone

4346C0 restores a previous nonidentical holder's saved matrix, then snapshots
the new one. Alias binding only refreshes baseline. 467D90 retains/releases;
holder state30 becomes2(nonnull)/0(NULL), unless bit8 preserves the whole word.
Last-owner NULL destruction does NOT restore saved baseline. Shared UV updates
the last-bound holder, not necessarily the caller of material update.

UV clone41ABB0/copy434D60 copies clocks, saved matrix, embedded scalar runtime
state and pivot/axis. Material uses map-aware412C40. Unbound stays unbound;
pre-mapped uses supplied holder. Unmapped material recursively clones68 bytes;
its467DF0 copy uses **always-clone412BE0** for UV even with root UV mapped.
Thus a second UV clone is retained by the new holder, while returned root UV
has refcount0 and points to that holder. Both are torn down explicitly.
The412BE0/412C40 distinction was already established in BaseObject research.
Portable UV/Trans clone graphs remain explicitly refused: native observation
does not mean source clone parity. Host shared owners/detach avoid stale native
backlinks; those safety adaptations are separately labelled.

## Common codec / material integration

TransFunction nested field0 uses UInt8 reserved length, seven sequential scalar
sections, then24 bytes pivot/axis. UV field0 uses UInt32 reserved length around
the Trans section. Source codecs share existing SectionCursor/writer/reference
core. Scalar runtime time/clamp are not serialized. Raw codec IEEE states stay
representable; finite runtime guards are separate.
