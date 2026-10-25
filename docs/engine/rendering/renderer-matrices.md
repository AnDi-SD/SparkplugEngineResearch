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

## Shader consumers and dirty propagation

Constant descriptors consume this cache through `4AE930`:

| Descriptor | Consumer result |
| --- | --- |
| 1, `view_proj_matrix` | cached CB80 |
| 2, `view_matrix` | cached CB00 |
| 3, `VPTransform` | cached CB40 |
| 4, `inv_view_matrix` | rigid inverse of cached CB00 |

The first consumer with dirty set refreshes all three products and clears
dirty. A following consumer with clean cache does not recompute, even if raw
input words were changed directly. Setting dirty again makes the next consumer
refresh and observe the new products. The inverse consumer follows the same
lazy dependency; it does not invert raw CA80 instead.

Descriptor destination start/count still controls the copied register window;
words outside it remain untouched. This finite matrix-to-constant dependency
does not establish all x87 edge cases or constructor/reset behavior. The
[constant parameter contract](../materials/shader-constants.md) defines the
rigid inverse and other partial-register writes.
