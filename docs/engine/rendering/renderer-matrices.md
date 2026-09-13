# PC renderer matrix cache

| Destination (complete renderer) | Exact product order |
| --- | --- |
| CB40 | CA80 × CAC0 |
| CB00 | CA40 × CA80 |
| CB80 | CA40 × CB40 (newly computed) |

Then byteF2F4 becomes0. Direct4AD540 always recomputes, including clean cache.
Lazy4AD640/4AD680/4AD660 use secondary `this=complete+18`, testF2DC and
call4AD540 only when nonzero. They return completeCB00/CB40/CB80 respectively.
Changing raw input without setting dirty intentionally leaves old cached values.
Original shader names mapCB00=view_matrix,CB40=VPTransform,CB80=view_proj_matrix;
names must not override the observed product order.
