# PC ColorFuncEval,

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
