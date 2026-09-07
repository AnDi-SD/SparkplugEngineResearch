# Original RFX documents through PC engine callbacks — CP55

PC executable SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Both input files are read-only, hash-verified originals from the local corpus.

| Input | SHA256 | Original events | Variables | Passes |
| --- | --- | ---: | ---: | ---: |
| Fixed.rfx |ac6785428ba851dea88e3a6cddc03fdf620fa06d82063db83176598140803db1 |76 |5 |1 |
| Bumpmap.rfx |1deacdcd1c7692edcd5451ccd50b2b771af047ea80bb3b4fddf42750a510e245 |106 |7 |1 |

External ElementTree decoding supplies **all** start/end events and attributes
from each complete document. Original engine callbacks `4D4750/4D3A80` and
`4D6050`, typed variable/pass/constant creation and original teardown execute
without engine-success substitutions. This verifies the complete **engine
event stream**; it does not claim execution of the original whole file loader,
ID/name regex or XML-library wrapper. The latter was executed only on CP52-53
small documents. Template identity/document/initial field40 are explicit
consumer inputs for this probe, not recovered file-loader output.

Reconstructed C++ receives the same decoded events. Comparisons are exact for
all variable fields, unknown-field markers, shader code/declarations/entry/
target strings, version/stage flags, constants and final pass vector. No
shader source truncation or opaque replacement is used. Capture fingerprints
(SHA256 of ensure-ASCII compact JSON) are:

- Fixed: `c0f96b7beb74ba38528139700eb69e9f85b5bb73d8e406bc1e602c345b90df59`.
- Bumpmap: `c3e6ab295fcd1f5ea8cab04d61a324af1c69731e14a777a4eeb5ef699c63a3fb`.

The shader CODE attributes are8916 and5303 bytes. The corpus fixture admits
long strings only when they exactly match values from the verified input;
large imported char_traits copies must stay inside explicit input storage or
live tracked native allocations. This replaces arbitrary string scanning with
known storage extents, retaining generic short-string contracts elsewhere.
No native allocation/request/instruction/process limit is raised. Original
string size/capacity/terminator are checked when capturing full shader text.

`pc-rfx-corpus`:2/2 exact captures,184 native assertions (77+107). Fixed peaks
at23044 instructions per call and50896 arena bytes; Bumpmap23140/42352.
Fixed teardown frees every tracked allocation. Bumpmap leaves exactly two
out-of-line texture filename allocations, the same shared-only destructor
behavior independently established by CP54. No other ownership delta occurs.
Source safely releases those strings.

This connects CP52-54 engine behavior on actual resource documents. Source
specialization, compiler/reflection output, full template file acquisition and
the complete generating cache miss remain open. No new class is removed from
scope, no PS2 credit or full SMO/SAN rendering/startup claim.

Accounting correction: CP54's initial immutable manifest note said24 native
assertions; its actual profile totals23. This correction does not change class
scores or the8/8 comparison result.
