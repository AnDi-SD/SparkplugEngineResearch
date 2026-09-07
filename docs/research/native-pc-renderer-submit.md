# PC complete geometry submission — CP42

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original4BC4A0 executes all internal methods; external COM observers only.
This is an **unlit/no-blend-weight/NULL-texture consumer**, not whole SMO load
or live rendering. No GPU, compiler, internal fake-success or full ctor runs.

## Entry and buffer lifetime

Ten arguments at4BC4A0, in order: index buffer, vertex buffer, primitive kind,
start index, primitive count, base vertex, vertex count, declaration, stride,
component flags. Names are analytical roles, not recovered C++ signatures.
Secondary renderer4BC670(mesh) subtracts0x18 from this and extracts:

|Argument|Mesh offset|
|---|---|
|index/vertex buffers|54/58|
|kind/start/count|50/78/48|
|base vertex/vertex count|7C/4C|
|declaration/stride/components|84/74/44|

Changed declaration identityC9FC calls declaration slot8 with low-byte bool
boundVS==NULL, then stores identity. Actual PC4C9D00 ignores that argument
and submits handle18 through COM15C. Declaration is borrowed, not retained.

Changed index identityCA04 submits handle10 orNULL through COM1A0, then
releases the old intrusive uint16 ref8 (deletes if it reaches0), retains new
nonnull ref8 and updates cache. Changed vertexCA08 submits COM190
(stream0,handle10,offset0,stride), then performs the same ownership update.
Changing **stride alone does not submit again**. A changed NULL vertex is
unsafe in original; NULL index is explicitly supported. HRESULT is ignored;
the cache/ref update still happens after the COM call.

Source retains actual reconstructed buffer objects with shared_ptr, and
borrows the declaration. This is a safe host ownership analogue, not native
uint16 overflow emulation. Handles remain explicit consumer tokens; no
device buffer allocation/upload is implied. Tests use actual native buffer/
declaration factories with NULL handles and one declared caller reference.
Intermediate callback snapshots compare identities and retain/release order.

## Complete internal chain

After binding,4BC4A0 calls real4BE180 material install,4BB890 material states;
material state8==2 skips4BDE50 lights. Then COMC4 receives the copied17-word
material block. For every pass, in order:45F570(-1) updates layers,4BC410
applies texture states/final blend,4BC290 selects/draws. The automatic
no-weight branch executes real4BE310,4C8980 and4BE2B0 without a generating
miss. Internal false results propagate; COM HRESULT alone does not fail.
Zero passes still bind buffers/install material/submit COM material but do
not create the shader manager or issue a draw.

`SubmitUnlitGeometryForAnalysis` reuses all reconstructed common methods,
including cache maps, pass controllers and shader manager. It refuses lit,
weighted, unknown texture-resource or unsafe input paths instead of calling
an invented successful internal callback. Full non-NULL texture resolution,
lit/weighted combined submission, mesh-object source adapter, renderer
lifecycle/reset, actual buffers and complete file display remain open.
The mesh fixture executes native4BC670 and compares its observable result
to the source explicit-argument API; it does not claim a source mesh adapter.

## Verification

pc-renderer-submit6/6 exact groups,30 counted native checks; source78/78,
build59/59, CTest45/45 in32.44s. Report:
`local-data/results/bounded-native-runs/20260907T024745984953Z-pc-renderer-submit.json`.
Empty/one/two passes, failed HRESULT, repeats, declaration/buffer switches,
NULL index, changed-only stride and mesh input all checked. Maximum16,826
instructions and64,320 arena bytes;100k/2s/64KiB/32KiB/30s caps unchanged.

The initial mesh fixture incorrectly assigned fields in ascending order;
the resulting unmapped declaration0x801 was a fixture input error, not a
native instruction/time cap. It was corrected against4BC670 before passing.
An empty-pass teardown initially tried deleting NULL shader manager; this
host cleanup error was fixed by filtering NULL (native empty submit passed).
No protected/capped path was resumed. No PS2 assessment or gate closed.
