# PC material color — CP25, 2026-09-06

Bounded **declared-state consumer** evidence. This is neither a native factory
nor a complete resource-loading result. PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Identity and the unresolved constructor

Original name `spMaterialColorController`, not `spMatColorController`.
ID `4C633E85`, base `spRenderController/14477AC7`, registration `75D788`,
initializer `6D1EB0` (catalog call `6D1ED0`), vtable `6DEBD4`:
`437610,5B7A00,41AF70,437630,4373D0,408350,408370,423190,4373E0`.

Protected factory `41A580` capped at `88CD04` in CP23. It and its calling
virtual clone `41AF70` remain disabled. Static IDA analysis of the protected
bridge is not constructor reconstruction. Deleting destructor `437610` calls
protected `4372E0`; complete destructor is also unproven.

The consumers address borrowed material `+24`, four saved RGBA vectors at
`+28/+38/+48/+58`, ColorFunc leaves at `+68/+B8/+108/+158`, and FunctionEval
alpha at `+1A8`. **1E0 is the minimum consumer-derived backing extent, not a
confirmed factory allocation.** Tests declare that zeroed backing and copy
actual leaf-factory states into the embedded regions. They never destroy this
backing as a native object or register it as a real animation-manager owner.
All real leaf temporaries, materials, serializers and managers are destroyed.

## Binding and update

`423650` restores the previous material's saved ambient/diffuse/specular/emissive
when changing to another pointer, including NULL. A same-pointer bind does not
restore: it refreshes the four saved values. Secondary material interface at
`complete+14`: ambient set slot8, diffuse10, specular20, emissive18.
`423A50` (static confirmation here) retains/releases material's controller74,
calls binder on non-NULL including alias, and skips binder for NULL.

`4373E0` consumes the render clock through `4231A0` and applies channels in
the order **ambient, diffuse, alpha, specular, emissive**. Activation is each
embedded scalar's **functionType != 0**, not its clamp-enabled byte. Type0
ignores even a nonzero Y offset; type7 is active constant output.

Four ARGB outputs normalize with the actual float32 `1/255` constant at
`6DCA9C` via `424700` / `437260`. Diffuse RGB may change while its previous
alpha is preserved. Only the separate active scalar alpha evaluator replaces
that alpha, clamped to `[0,1]`. Native always reads and sets diffuse even if
all evaluators are inactive. The order matters for the shared random stream.

Host `spMaterialColorController` composes existing ColorFunc/Function/Render
classes. Its **registered resource factory stays NULL**: analytical declared
objects may run methods/codec, but the loader cannot silently invent constructor
behavior. Clone/copy API refuses until the full graph is available. Host
material destruction/replacement clears stale backlinks if a context pins the
controller; this guard is not native destructor parity. Nonfinite/unbound
updates fail safely; partial failure side effects are not claimed identical.

## Independent copy consumer

`437630` was executed with declared source/destination, real clone manager and
only NULL or **already mapped** material references. Unmapped recursion is
forbidden because it can enter the capped factory. It copies render clocks,
borrowed mapped material, saved RGBA, four ColorFunc endpoints and embedded
Function states through `434730`. It **does not copy or reset alpha1A8**.
Destination alpha remains deliberately distinct from source.

`434730` copies clamp byte at30, not its three padding bytes; it also does not
copy the vtable or base padding. An initial overly broad byte comparison caught
this and was corrected to exact nonpadding ranges, consistent with disassembly.
No tolerance or execution limit was increased.

## Serializer

`spMatColorControllerSerializer/0F881A36`, target4C633E85,
registration75EE30, initializer6D3090, actual factory441200 (14 bytes),
read4412E0/write441740, primary6E2064/interface6E2058.
Field0 contains four terminated ColorFunc sections then one terminated scalar
section. Original writer always reserves **UInt32** length for field0, even
when input uses UInt8. Default payload is five terminators, then outer term.
Nondefault nested writer bytes match the common reconstructed codecs exactly.
No references are encoded inside these five leaves.

Source uses common SectionCursor/DataBlock plus the same leaf serializers,
including shared raw scalar-state field application. Full Material field6
loading/indexing/writing and DX per-frame consumer are the next boundary;
they are not claimed here from a substituted constructor.

## Verification

10/10 runtime/bind/codec comparisons, **58 native assertions and ten exact
captures**, source **98/98**, build42/42 +2/2, CTest **37/37**, 26.35 seconds.
Copy null/mapped: **22 additional native assertions**. Total80 native.
Maximum runtime19,036 instructions; nested codec14,291; copy7,308.
Arena at most1,936 bytes across these probes. All tracked real allocations
freed. Existing 100k/2s call,30s child,64KiB/32KiB bounds unchanged; no GPU.

One compile mismatch (`long double&` versus existing `double&` wide scalar
API) corrected. A source-test assumption that CPU MaterialData Clone would
refuse a controller was corrected: its known no-op copy intentionally yields
a blank material; the unproven controller's own Clone refuses.

Evidence: `research/probe_pc_material_color.py`,
`research/compare_pc_material_color.py`,
`research/probe_pc_material_color_copy.py`,
`Sparkplug/Tests/spMaterialColorTests.cpp`, shared source classes/serializers.
PS2, constructor/destructor, source clone graph, malformed native failures,
whole-file load/save, rollback/lossless unknowns and live renderer remain open.
