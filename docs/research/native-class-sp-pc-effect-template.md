# spPCEffectTemplate / spPCRFXFileLoader — PC, CP52

Evidence: pristine `WinxClub.exe`, SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
No PS2 credit. The exact class/TU paths occur in binary strings:
`Z:\Sparkplug\Code\SparkplugPC\spPCEffectTemplate.cpp` and
`Z:\Sparkplug\Code\SparkplugPC\spPCRFXFileLoader.cpp`; header/API spellings
and the portable data model are analytical, not claimed original source/ABI.

## Constructor, ownership and dispatch

`spPCEffectTemplate` ID `30E058FF`, base `spBaseObject`, vtable `6F3A64`.
RTTI has no factory; its clone virtual returns NULL. Constructor
`4D0960 -> 4AE520` takes identity, an MSVC string containing the **document**,
and a kind. In the `68`-byte original object these become `+10`, string `+24`
and `+1C`. Byte `+18` is false. Owned pointers `+14`, `+20`, `+64`, pass vector
`+48/+4C/+50` and variable vector `+58/+5C/+60` start empty. Fields `+40`,
`+44`, `+54` and padding are not initialized by this constructor; no default
meaning is invented. The separate name pointer is `+20`.

Destructor `4D0A10 -> 4D07D0` releases document, both independent owned text
pointers, pass records, variable records and child `+64`. CP52 executes empty
and populated **pass/string** ownership. Populated variables/child ownership
remain only static observations here.

Initialize `4D0650 -> 13CADF0` dispatches kind1 through an actual parser
success stub `4D74A0`, kind2 through RFX `4D6090`, and other kinds through
the success fallback. Success sets byte `+18`. There is no ready guard:
two calls parse twice and append duplicate passes without clearing the vector.

RFX factory `4D54B0 -> 13DACC0` allocates `728`; constructor
`4D5410 -> 13C8FB0` first constructs `spParser`, then its embedded `1DC`-byte
pass at `+548`. ID `01A95832`, vtable `6F3DA4`, RTTI `765618`, base `spParser`.
Clone `4D5510` creates a fresh empty loader, registers it, calls the inherited
no-op copy virtual and returns it. Destructor `4D5490 -> 4C9790` frees the
embedded pass and base parser. Original clone/lifetime run is included.

## XML-to-pass slice

`4D6090 -> 52FF90` creates the in-image XML-library parser (`6B14C0`), supplies
reader `4D0E80`, start callback `4D4750`, end callback `4D6050` and loader
userdata, parses (`6B52D0`), then destroys the library parser (`6B1680`).
The reader copies up to requested capacity, advances the document pointer,
and reports EOF when short. The start callback executes `4D3A80`, querying
decoded attribute records through `6B1720`. CP52 runs this entire original
small-document chain; only identified imported CRT/string memory operations
have fixture implementations. The XML library's source identity is unproven.

On end tag `RmPass`, `4D5FC0 -> 13BEAA0` copies the embedded pass into the
template vector, then `4CFFC0` resets it. Each pass has vertex shader at
`+D4` and pixel shader at `+158`; each shader has four MSVC strings at
`+0/+1C/+38/+54` (code/declarations/entry/target), bytes at `+70/+71/+72`
(minor/major/assembly), and parameter vector at `+74`.

Start `RmHLSLShader` requires `PIXEL_SHADER`; exact `TRUE` selects pixel,
all other supplied strings select vertex. Missing selector skips the record.
Present `CODE`, `DECLARATION_BLOCK`, `ENTRY_POINT`, `TARGET` overwrite the
corresponding strings; absent attributes preserve previous strings. Assembly
byte becomes zero. Exact targets map as follows:

| Target | Minor | Major |
| --- | ---: | ---: |
| `vs_1_1`, `ps_1_1` | 1 | 1 |
| `vs_2_0`, `ps_2_0` | 0 | 2 |
| `ps_1_4` | 4 | 1 |

Unknown/missing target preserves the previous version bytes. `RmShader` uses
the same selector, updates code only if present and sets bytes `1,1,1`.
`RmPass` attributes do not populate shader records. `RmState` reads NAME but
does not apply state in the observed branch; `RmTextureReference` is ignored.
`4CFF40` resets four strings and three flags but **does not clear parameters**;
populated parameter persistence is the next checkpoint, not CP52 closure.

## Reconstruction and verification

`spPCEffectTemplate` and `spPCRFXFileLoader` implement this engine callback
slice. C++ receives already decoded XML events; the comparison harness uses
ElementTree to provide that external-library boundary. It is not a recovered
standalone XML decoder. Native captures instead execute the original in-image
XML library, including entity/newline decoding and lifetime. Namespace/error
semantics are not claimed equivalent. Unsupported known variable/constant
branches explicitly return analytical incomplete.

`pc-effect-template` compares 18 complete captures: seven constructor/type
cases and eleven XML cases (empty/pass/HLSL, all five targets, unknown target,
missing selector, assembly and ignored nodes, with shared cases counted once).
One additional child tests actual RFX clone. Original assertions: 171; C++
unit assertions: 39. The unchanged bounds are 100000 instructions/2s per
call, 30s per child, 64KiB arena and 32KiB allocation request. No OS/GPU call.

Open: full RFX file read/name/ID regex handling; variable creation, metadata,
states and constants; malformed XML/error propagation; populated reload and
all ownership branches; source specialization, compile/reflection and the
complete cache-miss path. No full SMO/SAN render or live startup claim.
