# Whole PC shader-manager generating miss — CP57

Pristine PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
The **entire** original `4C8980` now runs through factory, template source/
compiler-output handling, vertex-device creation, cache insertion and return.
Only identified sprintf, SDK and device interfaces are external fixtures.
Template and renderer device slot are explicit prepared caller state. This
does not include original file acquisition or renderer/OS/GPU initialization.

## Key to source

After the already-confirmed low-weight-zero bypass and byte-key cache lookup,
a miss constructs two strings. The insertion block is, in this exact order:

- `BlendWeightCount = (mask &15);`
- `ColorMode = ((mask >>16)&15);`
- `LightCount = ((mask >>20)&15);`
- `bUseSpecular = ((mask >>24)&1);`
- one `LightType[i] = ((lightWord >>(2*i))&3);` for each encoded light;
- eight `bHasUVTransform[i] = ((mask >>(8+i))&1);` lines.

Each assignment has spaces around `=`, semicolon and LF. Header contains
`#define USE_TEXCOORDi` plus LF for `i < min((mask>>4)&15,8)`.
Source always uses the **first pass's vertex shader** from owned template40.
Original `417B20` returns static75AC68; its begin/end callees are real empty
stubs `5B7A00 ret4` and `48EAA0 ret`. They execute, not fixture-return success.

## Connected original path

`4C8980 -> 4C9F10 -> 4CFFE0 -> SDK boundary -> 4AF940 -> 4CA030 ->
device CreateVertexShader -> 4C87A0 -> cache result`

The template compiler return is ignored by the manager. On the verified
successful-code path, original PC vertex creation also ignores device HRESULT
and always returns1. Thus even a failed device result with NULL output still
caches/returns a non-NULL shader object whose device handle is zero. A failed
result supplying a handle caches that handle and releases it at teardown.
Repeat lookup returns the exact same shader without another compiler/device
creation call; zero-weight bypass still returns NULL before lookup.

Manager destructor `4C9370` owns template40: it destroys the template first,
then every cached shader and finally the map. Native tracked engine allocation
cleanup and expected SDK/device releases all match, including generated code.

Reconstructed spPCShaderManager now owns an explicitly supplied fixed template,
builds both key-dependent strings and provides `SelectOrCreateForAnalysis`
through the recovered template compiler consumer and existing PC vertex
device boundary. Existing cache-only API remains available. Missing required
caller inputs and unresolved compile-error/reentrant insertion paths remain
analytical incomplete. Setter replacement/reload is deliberately unclaimed.

## Verification and limits

`pc-shader-generation`:6/6 exact captures,53 native assertions (five cases9,
NULL failed-device case8). Source generation harness performs4 assertions per
case. Covers basic key, mixed fields and UV bits, texture-count cap8, all15
light entries, unused high bits, cache hit/bypass and both failed-device-output
forms. Maximum34158 instructions/call and59072 bytes of the unchanged64KiB
arena. The renderer backing is only the required C9EC-byte prefix containing
the consumed device pointer, not a claimed complete constructed renderer.

SDK bytecode/reflection and device handles are explicit test outputs; no
claim of GPU-valid compiler output, live display or full resource-to-render
execution. Full file/regex/initialization, missing compiler output, repeated
template replacement and reentrant same-key generation remain open. Renderer
automatic draw still needs this new generation API wired and verified as a
connected consumer; the cached path was independently covered earlier.
