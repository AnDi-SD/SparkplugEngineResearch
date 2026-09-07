# spParser: PC text normalization and lifetime

CP51, 7 September 2026. PC executable SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Class ID `3EC20087`, direct base `spBaseObject`; registration `7655B8`,
RTTI getter `4D0C90`, vtable `6F3A94`. Class name is exact; original source
module/header/API were not recovered. `Code/Sparkplug/spParser.*` is inferred
placement; analytical method names do not claim original API names.

## Verified behavior

- Factory `4D0DA0` resolves to `13DAAB0` and requests `0x538` bytes.
  Constructor `4D0CA0` resolves to `13C8070`; text ownership byte `10` is false,
  fields `14/18/1C/20`, vector `128/12C/130`, word `134` and `400` bytes from
  `138` are zero. Bytes `123..127` remain allocator data.
- The `255`-byte lookup table at `24` marks decimal codes
  `9,10,13,32,33,40,41,42,43,44,45,47,58,59,60,61,62,63,91,93,94,123,125,126`.
  This delimiter table is not identical to the normalization dispatch table.
- Virtual text method `4D0BF0` sets input begin/end/length, sets word `20` to
  one and calls `4D0AA0`. The latter allocates exactly the supplied length,
  scans through the inclusive end, appends NUL, owns the new text and stores
  its size including NUL. **It leaves field `18` pointing to the old input end.**
- Tab and space collapse to a single space unless previous or next character
  is marked as a delimiter. Leading/trailing spaces can remain. CR and LF are
  discarded by the control-character branch, not converted to spaces.
- `//` skips to LF; `/*...*/` is deleted without an inserted separator.
  Therefore `a/**/b` yields `ab`. Quoted text preserves spaces, comment markers
  and line breaks. Backslash does not escape a quote in this method.
- Clone `4D0E00` makes an empty parser and uses inherited no-op copy `40ECE0`.
  Original destructors `4D0D80/4D0C30` release owned normalized storage and the
  vector at `128`. All tracked allocations are released in the bounded cases.

Factory `4D54B0` additionally confirms `spPCRFXFileLoader`: size `728`,
vtable `6F3DA4`, same parser prefix, zero `538/53C/540/544/724`, embedded
structure at `548` containing empty strings/vectors. Construction reaches
`13C8FB0`, `4D5120` and `4D3800`; destruction uses `4C9790`. This preliminary
lifetime witness does not claim a reconstructed complete RFX loader.

## Boundaries and open work

The supplied input length in these probes is strlen, with an explicit readable
NUL. A nonshrinking input needs length+1 bytes, although the native allocation
request is length. The zero-length fixture allocates one physical byte while
recording the original zero request. The C++ reconstruction deliberately owns
enough storage and rejects an unterminated quote. Unchecked repeated-load,
signed-character indexing, malformed quotes/comments, token extraction,
grammar/parser state and populated vector behavior remain unclosed.

No game, OS or GPU call is forwarded. Only allocation/free, nonthrowing SEH
storage and explicit imported MSVCP71 char-traits contracts are fixtures.
Original constructors, comment/quote/space branches and destructors execute.
The initial RFX scout stopped at missing char-traits import `6D920C`; after
identifying that import it passed with the already existing char-traits fixture.
No instruction/time/memory cap was raised.

Reproduction: `python research/native_workbench.py run pc-parser`.
CP51 validation: 11 exact native/source captures, 118 counted native assertions
across 12 bounded children (including the separate RFX factory witness),
33 C++ assertions. Report:
`local-data/results/bounded-native-runs/20260907T050658056149Z-pc-parser.json`.
Source: `Sparkplug/Code/Sparkplug/spParser.cpp`; native probe:
`research/probe_pc_parser.py`; exact comparison: `research/compare_pc_parser.py`.
This is a bounded behavior slice, not full parser or shader readiness.
