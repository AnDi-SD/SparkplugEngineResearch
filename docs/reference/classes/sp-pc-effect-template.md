# spPCEffectTemplate / spPCRFXFileLoader

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPCEffectTemplate](../../../Sparkplug/Code/SparkplugPC/spPCEffectTemplate.h), [spPCRFXFileLoader](../../../Sparkplug/Code/SparkplugPC/spPCRFXFileLoader.h).

## Constructor, ownership and dispatch

`spPCEffectTemplate` ID `30E058FF`, base `spBaseObject`, vtable `6F3A64`.
RTTI has no factory; its clone virtual returns NULL. Constructor
`4D0960 -> 4AE520` takes identity, an MSVC string containing the **document**,
and a kind. In the `68`-byte original object these become `+10`, string `+24`
and `+1C`. Byte `+18` is false. Owned pointers `+14`, `+20`, `+64`, pass vector
`+48/+4C/+50` and variable vector `+58/+5C/+60` start empty. Fields `+40`,
`+44`, `+54` and padding are not initialized by this constructor; no default
meaning is invented. The separate name pointer is `+20`.

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
