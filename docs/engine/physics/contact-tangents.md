# Contact tangent value helper

PC helper `48B220..48B2D6` takes pointers to a three-float input and two
three-float outputs. Its original declaration owner and C++ return type are
unknown. The portable API `ComputeContactTangentsForAnalysis` is our free
value function; it does not add a member to a native constraint class.
The caller at `48B990` belongs to the separate ContactConstraint method `20`.

The branch compares signed input Z directly with the float word `3F3504F3`
(about `0.70710677`). There is no absolute-value operation. For Z greater
than this threshold, the selected sum is `S=Y*Y+Z*Z`; otherwise it is
`S=X*X+Y*Y`. Both branches use original square root and reciprocal arithmetic.
S remains extended precision before square root, including when a float
store of the same sum would underflow to zero.

With `q=1/sqrt(S)`, the first branch writes
`first=(+0,-Z*q,Y*q)`, then
`second=(S*q,-storedFirst.Z*X,storedFirst.Y*X)`.
The other branch writes `first=(-Y*q,X*q,+0)` in X/Z/Y order, then
`second=(-storedFirst.Y*Z,storedFirst.X*Z,S*q)`.
The stored first output is reread, so its float rounding participates in
the second output. Literal zero stores are positive zero; other zero signs
follow the original arithmetic. Neither vector is silently normalized again.

The default profile refuses. Explicit `PcFiniteNearest037F` admits finite
inputs with magnitude at most 256 and positive selected S. This excludes,
for example, `(0,0,-1)`, whose selected XY pair is zero under the original
signed-Z branch. Admission is our analytical boundary and preserves both
outputs when refused. It does not replace the native zero-division result.
Input is copied by value; native aliasing and within-call mutation are open.

The narrow integer arithmetic models nearest rounding with 64 significand
bits and the original float stores. Its shared Number-root entry remains
in the historical Sphere arithmetic header and also serves this second
bounded value query; no second root, reciprocal or product algorithm is
introduced. Portable C++ memory layout, FPU status/exception behavior, other
rounding modes, broader inputs, physical validity, the caller composition,
PS2 and connected runtime remain separate.
