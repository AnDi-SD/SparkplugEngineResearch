# PC renderer matrix setters/raw getters

| Slot | Entry | Complete destination | COM+B0 transform ID | Raw getter |
| ---: | --- | --- | ---: | --- |
| 12 | 4BBAE0 | CAC0 | 3 | 4AD370,slot18 |
| 13 | 4BBB20 | CA80 | 2 | 4AD360,slot17 |
| 14 | 4BBB60 | CA40 | 256 | 4AD350,slot16 |

Projection/view/world labels are analytical matrix roles; exact member/API
spellings are not surviving original names. Each setter calls actual41D330
to copy16 raw words, writes dirtyF2F4=1, then calls COM+B0 with the cached
matrix address. Always submitted, even identical repeated input or self-alias.
HRESULT ignored; nativeAL=1. World entry ret8 consumes an unused second
argument, not an implemented palette/index selector. Raw getters return
the input address and never trigger4AD540 or clear dirty.
