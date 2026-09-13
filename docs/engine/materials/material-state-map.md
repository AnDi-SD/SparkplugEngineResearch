# PC material-state translation

## Layer state output

Original protected423590 reaches49E4D0. Arguments `(stage, output)`;
stage is not used. It copies nine words from nested texture+10..30 to the
writable output in order and returnsAL1. No device/state application occurs
here. Defaults and arbitrary raw words match for stages0,1,2,FFFFFFFF.
Initial scout usedNULL output and faulted on49E4F1; corrected valid output
succeeds. This was an uncapped ABI/input fault, not a native hang/retry.

Source `spMaterialTextureLayer::CopyTextureStatesForAnalysis` copies the PC
nine-word prefix from shared portable storage, not all twelve PS2 words.
NULL nested texture returnsfalse as a host guard; original requires a valid
pointer. Partial overlapping output and malformed pointers are not claimed.

## Raw material state to device state

| Raw index | Value | Device state/value |
| ---: | --- | --- |
| 1 | any | 8 = value==1 ? 2 : 3 |
| 2 | 0..1 | 9 = value+1 |
| 3 | 0..2 | 22 = value+1 |
| 4 | any | 7 = value==1 |
| 5 | any | 14 = value==1 |
| 6 | 0..7 | 23 = value+1 |
| 7 | 0,1,2,3,4,6 | 19/20 = (2,1),(9,1),(5,6),(1,4),(2,2),(5,2) |
| 7 | 5 or >6 | no device calls; still true and raw cache updated |
| 8 | lighting mode | virtual slot35/+8C →4BDB10; separate pending frontier |
| 9 | any | 24 = raw value |
| 10 | 0..7 | 25 = value+1 |

Original4B0A90 compares the separate device cache `E4F4+4*deviceIndex`, calls
COM+E4 if changed, then updates that entry, ignoring HRESULT. The material
wrapper ignores that result too. Thus observers see **new raw state, old
device cache**. Repeating an identical value suppresses device calls even
after a failed HRESULT. Mode7's two mapped states are independently cached.
No direct raw-cache equality gate suppresses the translation itself.
