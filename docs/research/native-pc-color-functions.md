# PC ColorFuncEval — CP24, 2026-09-06

Status: substantial bounded evidence, not complete SMO/SAN or live rendering.
Original PC `WinxClub.exe` SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PS2 receives no credit from these observations.

## Identity and state

`spColorFuncEval` ID `0BC70FE7`, registration `760BD8`, initializer `6D3E50`
(catalog registration-call address `6D3E70`), base `spColorEval/39C85969`.
The latter registers at `760B18`, initializer `6D3DC0`, base
`spEvaluator/E91D088D`, no registered factory. Its complete contract is not
claimed by implementing the abstract host prefix.

Actual factory `478A60` allocates `50` bytes, vtable `6DE684`:
`42FAD0,5B7A00,478AC0,40ECE0,42FAC0,408350,408370,478990`.
Endpoints `+10/+14` default to PC `FF000000`; embedded `spFunctionEval`
occupies `+18..4F` and uses the already checked scalar state/defaults.
Clone `478AC0` invokes base copy `40ECE0`: changed endpoints/function state
are NOT copied; the registered clone has factory defaults. Actual clone-map
and deleting-destructor paths were exercised. The following vtable word
belongs to another class and is not counted as a ninth ColorFunc slot.

## Runtime

`478990(this,outARGB,delta)` invokes the embedded scalar evaluator, then
rounds `(wideScalar+1)*0.5` to float32 and clamps that weight to `[0,1]`.
First endpoint receives that weight, second receives its complement.
Helper `478920` multiplies **each of four bytes separately**, invokes actual
finite integer conversion `60DB90`, stores its low byte, then adds corresponding
bytes modulo 256. This is not one rounded interpolation. Equal white endpoints
at half-weight produce `FEFEFEFE`; equal default black produces `FE000000`.

Types 0/7 return the scalar Y offset without advancing its clock; the material
controller's separate type-0 disabling rule does not apply inside ColorFunc.
Random uses the shared PC MT19937 stream established in CP21. The host uses
the same wide scalar API, not another waveform/random implementation.

## Common codec

`spColorFuncEvalSerializer/2CC46B90`, target `0BC70FE7`, factory `47E240`,
reader `47E320`, writer `47E850`, registration `7612D8`, initializer `6D4180`.
IDs 0/1 are endpoint ARGB; 2 type; 3 frequency (and reciprocal update);
4 amplitude; 5 X offset; 6 Y offset; 7 pitch. Writer uses Fixed4 headers
`60..67` and suppresses PC defaults. Unknown fields skip; repeated fields
overwrite; quiet NaN payload bits survive; zero frequency stores reciprocal
infinity; negative-zero offsets compare equal to zero and are omitted.

Source reader/writer uses `spSectionCursor` / `spDataBlockSerializer`. Scalar
field application was factored into `ApplyRawStateFieldForAnalysis`, shared by
the scalar and color codecs. No alternative format reader was introduced.
The earlier schema helper retains its explicit platform-default parameter;
the actual executable codec implemented here is PC-only.

## Verification and limits

`pc-color-functions`: **20/20** fresh bounded children; **142 native assertions**,
**20 exact source/native captures**. Cases: real factory/clone, three endpoint
blends, five codec shapes, function types 0..9 with positive/negative deltas.
`SparkplugColorFunctionTests`: **118/118**, full build **231/231**, CTest
**36/36**, 24.02 seconds. Maximum runtime call 17,607 instructions; leaf arena
304 bytes, codec 384, clone 464. All tracked actual allocations freed.

No internal evaluator, codec, clone or conversion function is replaced.
Finite external `floor` boundary only; original integer conversion executes.
Limits remain 100k instructions/2s per call, 30s child, 64KiB arena and 32KiB
native allocation. Source rejects unsafe nonfinite runtime and malformed
envelopes; that safety behavior is not credited as native exception parity.
Exhaustive x87/NaN/overflow, full resource dispatch, rollback, lossless unknown
fields, material lifetime and whole-file/live renderer remain open.

MaterialColorController factory `41A580` separately capped at `88CD04` in
CP23 and is disabled. **Never retry/resume it or its factory-calling clone.**
Independent material consumers on declared state are not factory/load proof.

Evidence: `research/probe_pc_color_functions.py`,
`research/compare_pc_color_functions.py`, `Sparkplug/Tests/spColorFunctionTests.cpp`,
`Sparkplug/Code/Sparkplug/spColorFuncEval.*`, common scalar/color serializers.
