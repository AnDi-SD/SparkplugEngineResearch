# Sphere contact: bounded PC value helper

The common [value query](../../../Sparkplug/Code/Sparkplug/spBoundingVolumeQueries.h)
implements complete PC helper `472270..4723AD`: 318 bytes, three `RET` paths,
and no callees. Its original declaration owner is unknown. The portable name
`ComputeSphereContactForAnalysis` identifies an analytical free function;
it is not an extra virtual member of `spSphereBV` or a new registered class.
Sphere, Box, Capsule and OBB primary tables expose their existing transform
at `1C`; query function ownership is established separately.

## Arguments and contact result

The native cdecl call takes five words: pointers to two centers, their two
raw float radii, and a pointer to a 28-byte result. Argument order is
`center1, radius1, center2, radius2, output`. The helper uses the third
argument's stack home as a temporary after retaining its pointer in EDX.
The [PC views](../../../Sparkplug/Analysis/PC/spSphereContactAbi.h) describe
these function arguments and result fields separately from portable C++ ABI.

The result contains position at `00/04/08`, normal at `0C/10/14`, and depth
at `18`. Full EAX returns `0` or `1`. A false result leaves every output
byte unchanged. Exact tangency is false: the original tests a strict
inequality, not an inclusive overlap or an epsilon threshold.

The original first computes `(dz*dz + dy*dy) + dx*dx` from `center2-center1`,
then stores that squared distance as `float`. It compares the reloaded
value against the extended square of `radius1+radius2`. Thus the single
precision spill participates in both contact selection and later math.

On contact with stored squared distance equal to zero, it copies the exact
raw first-center words, writes fixed normal `(0,+1,0)`, and writes the
radius sum as depth. A small positive distance can underflow to this
stored zero and select that branch even when the centers differ.

For a positive stored squared distance, original `FSQRT` and reciprocal
supply the distance and normal. Normal direction is `center1-center2`.
The contact position is formed from
`center1 + normal * ((radius2-radius1-distance)*0.5)` and depth is
`radius1+radius2-distance`. Intermediate rounding matters: normal Y/Z
differences spill to float while X stays extended; the stored normal is
then reread. Point-offset X/Y spill to float while Z stays extended.

The positive-distance path publishes normal Z, X, Y, then position Z, X, Y,
then depth. The zero-distance path publishes position X/Y/Z, normal X/Y/Z,
then depth. Portable source mirrors these destination ranges and ordering;
it does not promise native instruction widths or store atomicity.

Radii are raw signed math inputs. The native body does not reject negative
radii: a negative sum can pass the squared test and produce a negative
depth. This behavior is preserved without asserting physical validity.

## Caller role and qualified arithmetic

The actual sphere-pair wrapper `472500` reads each CollisionInfo primitive's
radius at `34`, supplies centers from CollisionInfo `20`, and calls this
helper at `47252E`. On success that wrapper also writes identities at
`1C/20` and a tag at `24`; those extra fields are outside this helper's
28-byte output. Literal initializer `456B40` installs that wrapper at
`75FA50` and installs Capsule pair functions separately. These are caller
and role facts, not execution of a dispatcher or a full collision manager.
No synthesized Sphere or CollisionInfo record is labelled an exact native
constructor capture, and wrapper composition is unqualified.

The default profile refuses. Explicit `PcFiniteNearest037F` admits finite
input floats of magnitude at most 256, including signed zeros, subnormals
and negative radii. Admission results are distinct from native `0/1` math
results and preserve output. Input is copied by value; native aliasing and
within-call mutation are outside the qualified domain.

The [query-local arithmetic](../../../Sparkplug/Analysis/PC/spSphereContactMath.h)
models nearest rounding with 64 significand bits and the native float
stores. Existing exact add/normalize/store helpers are reused unchanged.
A bounded 128-bit integer root and division implement the correctly rounded
positive float square root and its extended reciprocal; a 64x64 product
retains the extended products. Native comparison cases execute original
`FSQRT`/`FDIV` without a host answer or imported-math callback. This is an
explicit arithmetic model, not a universal x87 exception/status environment.

Other rounding modes, NaN/Infinity, broader magnitudes, portable/native ABI equivalence,
Sphere factories, owning object graphs, the pair wrapper, broadphase,
physical contact validity, PS2, connected runtime and whole-class closure
remain open. The actual five-argument cdecl/RET0 call is established separately.
Existing transform acceptance is separate and unchanged.
The [driver](../../../Sparkplug/Tests/spBoundingVolumeQueriesTests.cpp) covers
result bits, strict false-result preservation, numerical branches, source
admission and input/output canaries.
