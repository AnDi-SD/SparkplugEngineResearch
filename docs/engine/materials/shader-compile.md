# PC template source and compiler-output consumer

## Source specialization

The function takes shader-record pointer, output spDXShader pointer, and two
MSVC strings **by value** (insertion, header), returning AL and popping40hex
argument bytes. Its two `4CFE40 -> 13B96C0` calls build:

`header + shader.declarationBlock + shader.code`

It searches that entire concatenated string for the first exact
`// INSERTION POINT`. If found, `4CFCD0` inserts the insertion string **before**
the marker, preserving the marker and all following text. There is no automatic
newline. A marker in header/declarations takes precedence over one in code.
Missing marker or empty insertion leaves the concatenation unchanged. Both
by-value arguments and intermediate strings are destroyed by the callee.

## SDK boundary and result consumption

Byte73FE6B is set1 before calling the selected compiler. Shader byte72 selects
assembly (`611241`,7 arguments) or HLSL (`611324`,10 arguments). Both receive
the exact text and length, NULL macro/include pointers and flags0; HLSL also
receives entry/target strings. The ABI, buffer outputs and reflection table
match the documented D3DX9 library contracts:
[D3DXAssembleShader](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxassembleshader),
[D3DXCompileShader](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxcompileshader).
Attribution of these statically linked entry addresses follows their matching
binary interfaces; an exact SDK build/version is not claimed.

Assembly appends every parameter already stored in the shader record through
`4AF940 -> 45A6C0`, even before testing the returned code pointer. HLSL uses
returned constant-table GetDesc, enumerates GetConstant(NULL,index), requests
up to16 descriptions and consumes the first name/RegisterIndex/RegisterCount.
Both paths feed actual case-sensitive shader parameter resolution and append;
unknown names remain type0 and still contribute register count to field34.

The engine decides success from the code-buffer pointer, ignoring HRESULT.
Adversarial boundary cases with failed HRESULT and a supplied buffer confirm
this branch rule; they are not asserted normal SDK output. HLSL requires its
table on the successful path. It releases that table, clears byte73FE6B,
allocates engine-owned code, copies all bytes (including non-dword tail),
stores size/pointer at output48/4C, releases any warning buffer and code buffer,
destroys strings and returns1. Native SDK call traces include three buffer
size queries; the source value boundary assumes a stable library buffer.
