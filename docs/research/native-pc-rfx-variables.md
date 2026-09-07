# PC RFX variable producer and ownership — CP54

Pristine PC SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Extends [CP53](native-pc-rfx-constants.md); exact TU remains
`Z:\Sparkplug\Code\SparkplugPC\spPCRFXFileLoader.cpp`.

`4D3A80` creates non-RTTI variable records. Shared constructor `4D2730 ->
48F3E0` initializes two MSVC strings at0/1C; it leaves other words untouched.
Shared attribute reader `4D34B0` requires NAME, copies it into **both** strings
and sets byte38 from exact `ARTIST_EDITABLE="TRUE"` (missing/other ->false).
DISPLAY_NAME does not replace the second string. Word3C selects type. Vector
append `4D36C0` / wrapper `4D38C0` does not reject duplicate names.

| Tag | Allocation/type | Recovered payload |
| --- | --- | --- |
| RmBooleanVariable |44/type1 |byte40: VALUE equals exact TRUE |
| RmFloatVariable |4C/type3 |float40 VALUE;44 MIN;48 MAX |
| RmVectorVariable |70/type5 |four float words40..4C; following eight words remain untouched |
| RmColorVariable |4C/type4 |packed ARGB at40;44/48 untouched |
| Rm2DTextureVariable |94/type6 |three strings40/5C/78; FILE_NAME sets first |

Numbers cross identified MSVCR71 `atof` IAT6D9308. Native stores float32 for
scalar/vector values. Scalar CLAMP missing leaves MIN/MAX untouched. Exact
TRUE independently reads whichever MIN/MAX exist; any other supplied CLAMP
sets0 and1000, ignoring supplied ranges. Color reads VALUE_0/1/2, multiplies
each double by255, truncates with original `60DB90`, stores the low byte in
R/G/B, and sets alpha255. VALUE_3 is ignored. Values outside0..1 wrap in the
observed finite signed32 conversion range; they are not clamped.

Texture constructor `4D29B0 -> 4B7420` builds its three additional strings.
Template destruction `4D07D0` calls **only** shared destructor `4CFC20` on
each variable, then frees its block. Thus the two shared strings are freed,
but an out-of-line filename string in the texture tail is leaked. A short
filename stays inline and does not leak. Native filled probes prove exactly
one remaining tracked allocation for the long-filename case and no other
ownership deltas. Portable C++ uses normal string ownership and releases it;
the native leak is documented, not reproduced as a host leak.

C++ exposes recovered payload words with explicit unknown optionals, not
guessed default values for untouched words. Native capture uses the explicit
CC allocator fixture to mark those untouched fields; the finite test inputs
do not collide with that sentinel. These are analytical structures, not a
recovered original header ABI. Missing mandatory attributes/error-object
branches and nonfinite/overflow conversions remain outside this slice.

`pc-rfx-variables`: eight exact post-event capture sequences,23 native
assertions. Covers booleans/case, scalar missing/default/partial clamp,
vectors, color truncation/wrap, short/long texture filenames, duplicate names,
mixed records, growth and actual template destruction. All callbacks,
constructors, string/vector methods and destructors execute original code;
only decoded XML attributes and finite CRT numeric conversions are explicit
library boundaries. Maximum13137 instructions, arena at most3184 bytes.

No credit for the full RFX file loader/regex, compiler/reflection, complete
shader-generating cache miss, full resource rendering or PS2 behavior.

Count correction: the immutable initial CP54 manifest note says24; the
actual per-child counts3+3+3+2+3+2+2+5 total23. Scores are unaffected.
