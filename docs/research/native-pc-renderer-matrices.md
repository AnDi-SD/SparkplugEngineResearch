# PC renderer matrix cache — CP46

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original protected4AD540 resolves through its actual guest resolver to
13D6A00..13D6ADF. Raw on-disk bytes after the dispatch are not the body.
The complete path calls original426B00 and41D330, with no external calls.

| Destination (complete renderer) | Exact product order |
|---|---|
| CB40 | CA80 × CAC0 |
| CB00 | CA40 × CA80 |
| CB80 | CA40 × CB40 (newly computed) |

Then byteF2F4 becomes0. Direct4AD540 always recomputes, including clean cache.
Lazy4AD640/4AD680/4AD660 use secondary `this=complete+18`, testF2DC and
call4AD540 only when nonzero. They return completeCB00/CB40/CB80 respectively.
Changing raw input without setting dirty intentionally leaves old cached values.
Original shader names mapCB00=view_matrix,CB40=VPTransform,CB80=view_proj_matrix;
names must not override the observed product order.

Source `spDXRenderer::RefreshMatricesForAnalysis` reuses the existing
`node_math::Multiply4ForAnalysis` (426B00 cell-specific summation order),
not a second math implementation. `RendererMatrixStateForAnalysis` is an
explicit analytical carrier in Analysis/PC, not a new original engine class.
Optional `spDXShader` input points to this carrier; types1..4 call the common
lazy getter. Older explicit cached fixtures remain available and clearly scoped.
Source refuses nonfinite multiplication inputs; wider intermediates preserve
the tested native cells, not every possible x87 exceptional/rounding behavior.

`pc-renderer-matrices`:5/5 exact captures,125 counted native checks,
12 seeded matrix triples ×2 reads per mode. Direct refresh, all3lazygetters,
and actual4AE930 type1→dirty refresh covered. Source123/123;
build72/72,CTest49/49 in44.86s. Report:
`local-data/results/bounded-native-runs/20260907T033501487606Z-pc-renderer-matrices.json`.
At most4191instructions/62960arena. Original factory shader/owned descriptor
deleted; declared renderer not claimed as full constructed renderer.
All previous native call/child/allocation limits unchanged; no GPU/OS forwarding.

PC DXRenderer74→75; no PS2 credit or class/gate closure. Input setters,
complete scene/world/skinning matrix producers and generated shader pipeline
are separate obligations. Type4 dirty composition follows the same verified
getter, but this CP's new native integrated capture specifically exercises type1.
