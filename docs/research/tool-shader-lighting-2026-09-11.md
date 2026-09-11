# PC selected-light constants and weighted OpenGL vertex lighting

Block 9, 11 September 2026. The Viewer now feeds the actual RenderNode light
cache into the shared PC shader constant producers and consumes those values
in its modern OpenGL backend. This closes the selected-light consumer for the
supported weighted `Fixed.rfx` path, not the complete game renderer.

## Source and boundary

The shipped `local-data/pc-pristine/Shaders/Fixed.rfx` has SHA256
`AC6785428BA851DEA88E3A6CDDC03FDF620FA06D82063DB83176598140803DB1`.
Its vertex entry targets `vs_1_1`. The existing CP93
[selected-cache-to-Skin evidence](native-pc-skin-selected-light.md) covers the
actual game selection/draw connection; no new original EXE execution or coverage
percentage is claimed in this block.

`spDXRenderer::ReadShaderLightForAnalysis` extracts the existing draw-caller
field projection into one shared function. Its source light, color, world/local
directions, position, attenuation and angles are unchanged. The draw caller and
the new host adapter both call it. This is a refactor, not a correction of game
behavior. Existing RendererSubmit/SkinRender checks pass after extraction.

`tools/SparkplugViewer.Native/ShaderLighting.h` calls actual
`spPCVertexShader`/`spDXShader::BuildConstantsForAnalysis` for material/ambient
products, light positions/directions, cone cosines and ARGB conversion. It uses
the actual selected cache; it does not filter selected disabled lights as if
they were fixed-function device enables. Only shader-consumed fields are
requested: a directional light's unknown attenuation does not become a guessed
zero, and the specular point branch does not require unused attenuation.

The C ABI `spv_scene_shader_lighting` publishes an atomic 836-byte snapshot,
including eight 96-byte light rows and explicit known masks. Unwritten original
register padding is not promoted to a defined constant. The current scope accepts
color modes 0–15; raw modes overlapping the original light-count key bits fail
explicitly. Missing scene/cache, wrong object types and non-finite consumed
inputs are rejected. The host supplies game-world-to-view and renderer constant
ARGB (Viewer policy: zero). Scene ownership retains graph lifetime.

## Modern backend

`SparkplugSceneRuntime.CaptureShaderLighting` only transports these constants.
The shared renderer borrows that runtime through `SetLightingRuntime`; Viewer
binds the same scene that owns its materials and animation. Repeated meshes
sharing a container/material reuse one capture within the frame. All frame
captures and borrowed references clear with the renderer.

`SmoGpuSceneRenderer.Lighting.cs` translates shipped vertex lighting to GLSL.
It retains the original ordering: non-specular point attenuation multiplies
the entire accumulated RGB; specular point lighting omits attenuation; the
non-specular spotlight keeps its signed diffuse contribution. Color modes 0–6
and the remaining lighting fallback follow `Fixed.rfx`. Color is clamped per
vertex before interpolation and then consumed by the existing texture stages.
Normals use the original direct matrix transform/normalization, without an
invented inverse-transpose replacement. Shader view remains left-handed
(+Z forward); GL projection uses the existing modern camera conversion.

The `vs_1_1 lit` operation uses positive dot products and clamps its exponent
to ±127.9961, following the documented device instruction contract:
[Microsoft lit-vs](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/lit---vs).
This is a modern device implementation; shader compiler/hardware bit identity
for all floating-point inputs is not claimed. Host Unlit/Wireframe inspection
modes suppress illumination explicitly.

Rigid geometry still uses the previously declared preview-light path; the
original rigid fixed-function lighting contract is a separate consumer.
Weighted materials without a bound scene cache report `SHADER_LIGHTING_PREVIEW`.
Custom shaders, complete visibility/frame scheduling, fog, shadows and game
global lighting outside the loaded document remain separate work.

## Verification and limits

Raw evidence is in
`local-data/results/tools-core-cycle-20260911-1900/shader-lighting/`; the manifest
`research/tools-core-shader-lighting-2026-09-11.json` binds the checked sources,
binaries and reports. Final native DLL SHA256:
`21F6E3D79E7FBFDE6E3CB2DC1CE2C1649DEA8C9E6D94C249C22E6A3BCBADA07F`.

- Native RendererSubmit/SkinRender/ShaderLighting suites: 3/3. The new host
  projection suite has 61 assertions, including all ordinary light types,
  transformed coordinates, disabled selection, unknown attenuation refusal
  and the specular branch that legitimately does not consume attenuation.
- Icy plus `xiwa.san`: 156 checks, 0.482 s, 45,285,376 bytes peak. Actual
  container ID3 selects ambient ID119; three material owners and five pose
  samples preserve their original worlds/palettes and shader constants.
- Production uniform publication plus production GLSL on RTX3070: 21 checks,
  1.056 s. Fixtures cover coloring modes, directional/point/spot branches,
  light order, negative spotlight diffuse and zero-dot specular rejection.
  These are bounded numeric fixtures, not captured whole-game GPU pixels.
- Actual Icy scene: 12 placements/12 passes, 7752 covered pixels at 192×192,
  no decode/material errors. Its selected ambient product is RGB zero and
  mode4 uses authored vertex color; toggling activation correctly changes no
  RGB pixels. The real cache is restored. PNG is retained and inspected.
  Warm live frames were 0.302/0.534/0.294 ms, peak 188,190,720 bytes. This is
  a small offscreen target, not a fullscreen performance claim.
- Existing material device tests: 31 checks. Actual Viewer load/SAN/slider
  handlers: 43 poses, 13,167 checks, 4.094 s, including a new requirement that
  each production frame has available materials and selected-light bindings.
- Managed builds and whitespace checks passed. Existing C4756 empty-sphere
  warnings appeared in the initial affected native rebuild; the new `strncpy`
  warning was removed by bounded length checking/copying.

New test mistakes were corrected without changing game arithmetic: ambient/
diffuse DTO ordering was initially swapped, ARGB decimal expectations ignored
the PC float32 reciprocal's one-bit rounding, and the first real pixel test
incorrectly required a black ambient product to change RGB. A first native
command named a nonexistent check suite; no build ran from that command.
The explicit exceptions and final scopes are retained rather than counted as
new original-code discoveries. The application cores are not all complete.
