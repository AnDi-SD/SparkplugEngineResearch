# PC spDXShaderLayer

## Actual factory, storage and ownership

Protected factory4ABDC0 succeeds in the unchanged bounded emulator, allocation
**28 bytes**. Vtable6F0B04:
`4B3200,5B7A00,4AC060,4B2FD0,4B31B0,408350,408370,423590,4D74A0,4F3DF0`.

Actual clone4AC060 creates/registers a new object, calls copy4B2FD0. Copy
clears destination, invokes inherited4235E0, copies borrowed14 unchanged,
then deep-copies both arrays of each parameter pair. Tests use zero, one,
three pairs, distinct counts1..3 and arbitrary raw float words. A separate
real nested MaterialTexture clone succeeds. Source mutation does not change
the clone. All tracked native allocations are freed.

Virtual slots8/9 (4D74A0/4F3DF0) are success-only stubs, accepting two/no
stack arguments. Reader helper5A7DB0 is also a one-argument success stub.
Inherited slot7/423590 requires a non-NULL texture and writable nine-word
output; a factory-empty shader layer does not establish that precondition.

## Negative codec finding — do not bypass the base check

Actual DXMaterialSerializer layer read4B12A0, write4B0F10 and index4B0EB0
first call common477230/477350/4767F0 respectively. Those common functions
accept specific Std/Env/Cube/Camera/Mirror/Movie RTTI families, not the base
MaterialTextureLayer. Exact DXShaderLayer has none of those bases.

With the **correct PC RTTI parent chain**, all three return false before
stream consumption/output or shader-manager work. These native rejections
are executed, not inferred from a missing host implementation. It would be
incorrect to skip the common check to make the following branch reachable.
This is a finding about these actual helper entries/identity, not proof that
no alternative runtime construction/processing path exists anywhere.

Unreached static tail4B12F8 onward reads a **u32 key** via416DA0 (not a string),
uses manager763024 (factory4C9680, lookup4AFFB0/add4B0680), and a resource
reference with expected ID394F7331. It stores layer14, reads a discarded
scalar via5A7DB0, processes material texture476890 and parameter arrays.
Do not credit this unexecuted tail as a functioning native shader codec.
