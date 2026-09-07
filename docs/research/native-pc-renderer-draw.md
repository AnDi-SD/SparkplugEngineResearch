# PC preselected-shader draw and shader stacks — CP37

Original PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual4BC290 with an explicitly non-NULL vertex shader atE454[E474].
Shader objects are consumer-declared54-byte inputs with handle+50, not
constructed shader classes. No internal shader method or generator is faked.
Only external COM observes binding/constants/draw; no actual GPU or OS.

## Command and cache order

The selected vertex shader is compared with bound identityE44C. Changed
identity is stored **before** COM+170 receives its+50 handle. NonzeroE444
always submits that many float4 registers fromCBC4 to COM+178 at register0,
including repeats. The native buffer/count total extent is not guessed.

Only when byteF2F5 is nonzero, selected pixel shaderE464[E478] is compared
with boundE450, cached before COM+1AC. NULL binds handle0. Pixel constants
E448 rows atD804 submit to COM+1B4 only with a non-NULL bound pixel shader.
Disabling F2F5 does not clear a previously bound pixel identity.

All seven input words are kept positionally until original API names are
recovered. Let them be `(kind,a2,a3,a4,a5,a6,a7)`:

| Native branch | Exact device command arguments |
|---|---|
|kind1|DrawPrimitive(1,a4,a3), COM144|
|kind0/2/3/4|DrawIndexedPrimitive(table[kind],a4,0,a5,a2,a3), COM148|

Table6F1A54 bounded indices0..4 is `[1,1,4,5,2]`. Index1 uses the separate
nonindexed branch. a6 is consumed only by automatic shader selection, not
this branch; a7 unused here. Every HRESULT is ignored and native returnsAL1.
Source rejects kind>4 rather than reproducing an unsafe native table read.

## Stack lifetime is separate from binding

4BE1B0 incrementsE474 then stores vertex shader atE454[top]. Protected
4BE1D0 decrements top without clearing the old element. Pixel equivalents
are4BE1E0/E478/E464 and4BE200. Helpers make no device calls/retain/release.
Bounded push/push-NULL/pop preserves stale entries exactly. Source four-slot
consumer stack has explicit overflow/underflow rejection; native helpers
have no such checks. This does not establish safe arbitrary nesting.

## Source boundary and verification

`spDXRenderer::DrawPreselectedForAnalysis` preserves the above order via
three external-device callbacks. NULL selected vertex shader **refuses**
the unimplemented automatic4BE310/4BE2B0 branch. It is not replaced by a
synthetic NULL shader and successful drawing. Shader/device opaque tokens
are explicit test input, not live host COM resources or shader factory proof.

`pc-renderer-draw`: **5/5 exact captures,39 counted native checks**. Primitive
types, pixel branch, constants, failed HRESULT and stacks. Capture original
arguments, cache/stack snapshots at each COM event, repeated draws and
shader changes/NULL pixel. Native assertions verify automatic generation
was never entered. Max95 instructions draw,1618 protected pop,63,152 arena.
Source **38/38**, build53/53, CTest**41/41**,35.90s.
Run `local-data/results/bounded-native-runs/20260907T012612299150Z-pc-renderer-draw.json`.

Fixture backingF2F8 and device-table1B8 expose only newly read offsets;
global64KiB arena and32KiB native-allocation caps are unchanged, as are
100k/2s call and30s child. Original files remain untouched. PC Renderer62→65,
PS2 unchanged; no class/gate is complete.

Next connected class is now identified with an **original source path**:
`Z:\Sparkplug\Code\SparkplugPC\spPCShaderManager.cpp`, classD5AE63DA,
base98BA76FE, record764CB0/initializer6D5A00. Actual4C9680 factory creates
50-byte object, empty destructor succeeds (8282 instructions/240 arena).
This standalone scout is not yet a source reconstruction or score credit.
Automatic key computation4BE310 calls PC manager slot7/4C8980; low key
nibble0 statically returnsNULL, nonzero may compile/cache a shader. Need
trace that distinction and complete buffer/material/pass submission.
