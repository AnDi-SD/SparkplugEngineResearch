# SwampGazBubble scalar properties (PC)

The separate five-property scalar facade takes a supplied 352-byte prefix with primary `0x006FE408`. Its finite-normal-or-signedzero profile admits all five selected binary32 fields and setter arguments. Setters copy the four argument bytes; getters supply the physical ten-byte extended value of the original `FLD` operation, including the sign of zero.

| Own publisher label | Offset | Setter | Getter |
| --- | --- | --- | --- |
| Max explosion radius cm | `+0x15C` | `0x00567EA0` | `0x0053A480` |
| Min explosion radius cm | `+0x158` | `0x00540D20` | `0x00558A50` |
| Max inflate speed cm/s | `+0x154` | `0x0054FDC0` | `0x00558A40` |
| Min inflate speed cm/s | `+0x150` | `0x00540CC0` | `0x00558A30` |
| Damage Radius (cm) | `+0x148` | `0x00540CA0` | `0x00556DE0` |

`ScalarCapturedPrefixForAnalysis` is distinct from the existing 336-byte RAW prefix. The Damage and Green APIs retain their original types and arbitrary-bit contracts. Scalar operations preserve the raw Damage word, Green byte and every other supplied byte. They stage outputs and leave them unchanged on refusal; setter input and output may alias. The shared `FromFloat` provider performs exact finite binary32 widening.

The original publisher labels do not impose ranges, units conversions or additional arithmetic. Historical allocation352, the selected352-byte scalar prefix and the earlier336-byte RAW prefix remain separate observations. This facade establishes no native constructor, C++ signature, sizeof, physical inheritance, lifetime, production caller, PS2 behavior, gameplay or whole-class completeness.
