# PC vertex shader and constant parameters/41

## Identity, construction and device lifetime

The first two registrations have NULL factories. Actual protected4C9F10
creates0x54-byte PCVertexShader, vtable6F2ECC, name10=NULL, words14..34=0,
allocator38 untouched, descriptor vector3C/40/44=0, bytecode48/4C=0 and
device handle50=0. No COM AddRef/device interaction in the factory.
Primary slots4CA060,5B7A00,4C9F80,413120,4C9F00,408350,408370,4CA030;
the following bytes are the class-name string, not additional virtuals.

Clone4C9F80 registers a clone and calls inherited named copy413120. Shader
state, descriptors, generated code and device handle are **not copied**.
The source restores this name-only behavior; it does not share device handles.

4CA030 receives already-assembled bytecode plus an unused second word.
It gets actual renderer75DB68/deviceC9E8 and calls COM+16C(device,code,&this50).
It always returnsAL1, ignoring HRESULT. It does not release an old50 before
overwriting it.4C9FD0 releases the final non-NULL50 through COM+8; base
4D61B0→4AF8E0 frees generated-code buffer4C and descriptor vector3C, then
the named base. Tests distinguish failure writingNULL, failure writing a
non-NULL output and repeated creation. The overwritten handle is not released
by the engine: preserving that fact is not an endorsement of this policy.
Caller-owned code/device callbacks are explicit source inputs, not compilation
or a live shader backend. Source refuses missing callbacks for host safety.

## Parameter identity and exact storage contract

Original4AE660 scans22 strings at6EFAE0, case-sensitive exact comparison;
unknown/empty or array-suffixed names return0. Every original mapping:

| ID | Name | ID | Name |
| --- | --- | --- | --- |
| 1 | view_proj_matrix | 12 | LightMatSpec |
| 2 | view_matrix | 13 | LightPos |
| 3 | VPTransform | 14 | LightDir |
| 4 | inv_view_matrix | 15 | LightInner |
| 5 | BlendMatrices | 16 | LightOuter |
| 6 | AmbientCol | 17 | UVTransform |
| 7 | ConstColor | 18 | ViewDirLightDir0 |
| 8 | MatDiffuse | 19 | LightAmbientColorDir0 |
| 9 | MatSpecular | 20 | LightDiffuseColorDir0 |
| 10 | MatSpecularPwr | 21 | LightSpecularColorDir0 |
| 11 | LightMatDiff | 22 | LightAttenuation |

Verified original dispatch and source:

- Types1/2/3 copy registerCount*16 bytes from cached matrices at complete
  rendererCB80/CB00/CB40. Actual getters4AD660/4AD640/4AD680 are called with
  secondarythis+18. Explicit dirty byteF2F4=0 prevents unrelated4AD540
  recomputation; this is an already-cached consumer test, not matrix startup.
- Type5 uses floor(registerCount/3) matrices from pointerC9B8, stride64.
  Each4x4 matrix is transposed; the first three rows are written as12 floats
  at startRegister+3*i. Remainder registers remain untouched. Tests cover
  counts0..6 and two nonzero/zero destination starts. Source finite-matrix
  guard explicitly excludes untested x87 NaN/Inf conversion edge cases.
- Type7 converts packedC194 ARGB to RGBA through437260 using the same exact
  float32 coefficient as the existing shared color helper.
- Types8/9 read actual selected materialE47C diffuse/specular getters, not
  the copied17-word fixed-function blockE4A4. Type10 reads its power and
  writes only the first float of the destination register.
- Types7/8/9/10 ignore registerCount, including0: one color or one power
  scalar is still written. All other destination words remain untouched.

This distinction matters for ordering: earlier4BE180 copies the fixed-function
material block **before** controller update, whereas these shader getters
read the selected material. No synthetic common block is substituted for both.
Power source requires explicitly initialized finite value; native uninitialized
power and unsafe destination/count cases are source guards, not new rules for
the original format. Inputs/outputs are bitwise compared, including untouched
0xA5 bytes, at register offsets0/2/3 and zero/nonzero counts.

## remaining14 parameter types

- Type4 calls461D40 on cached viewCB00. This is a **rigid transform inverse**,
  not a general matrix inverse: transpose upper3x3, last column0/0/0/1,
  translation negative dot products. A diagonal2/4/0.5 remains2/4/0.5;
  translation3/5/7 becomes-6/-20/-3.5. Native x87 grouping is preserved in
  widened source intermediates; no exhaustive x87 precision equivalence claim.
- Type17 reads UV matricesF0F4+64*i, floor(count/3) matrices. Helper461E60
  extracts upper3x3: each row writes only three floats; **the fourth word of
  each register is untouched**. Remainder registers also stay untouched.
- Type6 reads ambient light from listC190+20 and multiplies its RGBA by
  material ambient. Missing ambient gives all four zeros. A NULL light list
  itself is not declared valid. Types11/12 multiply each list light RGBA by
  material diffuse/specular; type22 copies light144/148/14C/E0 as four floats.
- Type19 instead multiplies rendererC178 ambient by material ambient.
  Type20 multiplies directionalF2F8 color by material diffuse; type21 returns
  **material specular unchanged**, not multiplied by light. Missing directional
  light gives0/0/0/1 for20/21. Types6/19/20/21 ignore descriptor count.
- Type13 transforms light position74 by complete viewCA80 (not cachedCB00).
  For typeC0==0 it transforms world directionA4 without translation, rounds
  the float3 intermediate, then scales by-1e9; w=1. Other types use position
  and translation. Type14 transforms world directionA4 as float3; w=0.
- Types15/16 write only the first float: cos(innerE4/outerE8 *0.5), **radians**.
  The half-angle coefficient6DC3D4 is3F000000, not a degrees conversion.
- Type18 uses directional light's **local direction58**, viewCA80 with XYZ
  translation zero, and full fourth-component computation; a missing light
  gives0/0/0/1. This differs from type14's world directionA4 and w=0.
