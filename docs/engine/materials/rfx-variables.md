# PC RFX variable producer and ownership

`4D3A80` creates non-RTTI variable records. Shared constructor `4D2730 ->
48F3E0` initializes two MSVC strings at0/1C; it leaves other words untouched.
Shared attribute reader `4D34B0` requires NAME, copies it into **both** strings
and sets byte38 from exact `ARTIST_EDITABLE="TRUE"` (missing/other ->false).
DISPLAY_NAME does not replace the second string. Word3C selects type. Vector
append `4D36C0` / wrapper `4D38C0` does not reject duplicate names.

| Tag | Allocation/type | Recovered payload |
| --- | --- | --- |
| RmBooleanVariable | 44/type1 | byte40: VALUE equals exact TRUE |
| RmFloatVariable | 4C/type3 | float40 VALUE;44 MIN;48 MAX |
| RmVectorVariable | 70/type5 | four float words40..4C; following eight words remain untouched |
| RmColorVariable | 4C/type4 | packed ARGB at40;44/48 untouched |
| Rm2DTextureVariable | 94/type6 | three strings40/5C/78; FILE_NAME sets first |

Numbers cross identified MSVCR71 `atof` IAT6D9308. Native stores float32 for
scalar/vector values. Scalar CLAMP missing leaves MIN/MAX untouched. Exact
TRUE independently reads whichever MIN/MAX exist; any other supplied CLAMP
sets0 and1000, ignoring supplied ranges. Color reads VALUE_0/1/2, multiplies
each double by255, truncates with original `60DB90`, stores the low byte in
R/G/B, and sets alpha255. VALUE_3 is ignored. Values outside0..1 wrap in the
observed finite signed32 conversion range; they are not clamped.

No credit for the full RFX file loader/regex, compiler/reflection, complete
shader-generating cache miss, full resource rendering or PS2 behavior.
