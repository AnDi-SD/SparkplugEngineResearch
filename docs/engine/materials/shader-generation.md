# Whole PC shader-manager generating miss

## Key to source

After the already-confirmed low-weight-zero bypass and byte-key cache lookup,
a miss constructs two strings. The insertion block is, in this exact order:

- `BlendWeightCount = (mask &15);`
- `ColorMode = ((mask >>16)&15);`
- `LightCount = ((mask >>20)&15);`
- `bUseSpecular = ((mask >>24)&1);`
- one `LightType[i] = ((lightWord >>(2*i))&3);` for each encoded light;
- eight `bHasUVTransform[i] = ((mask >>(8+i))&1);` lines.

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

Existing cache-only API remains available. Missing required caller inputs and unresolved compile-error/reentrant insertion paths remain analytical incomplete. Setter replacement/reload is deliberately unclaimed.
