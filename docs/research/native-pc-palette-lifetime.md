# PC palette, texture ownership and runtime failure contracts

Checkpoint 17, 2026-09-06. Original executable SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
This is evidence toward the active PC SMO/SAN goal, not completion.

## spPalette identity, construction and copying

Class `spPalette`, ID `591C0B9F`, direct Base `415352A1`; record `763DE0`,
initializer `6D52E0`, factory `4B2C80`, vtable `6F0A6C`, size **414**.
Index at 10 initializes to `FFFFFFFF`; 1024 entry bytes at 14 remain unwritten.
The diagnostic expression `pPalette->GetPalette()` and `256 * sizeof(spColor)`
provide an original accessor/type-name anchor; exact header/TU path and full
source signature are not recovered. Bytes are preserved without guessing channels.

Two different native copy paths are now independently executed:

- Copy constructor `4B2C50` copies all 1024 bytes but resets index/base state.
- Virtual clone `4B2CF0` factory-constructs a blank palette, records clone pair
  through original `412F70`, then calls no-op copy slot `40ECE0`. Its entries
  remain constructor-uninitialized. Do not implement it as the copy constructor.

The first clone scout lacked static clone-map startup and faulted reading NULL+4;
the corrected fixture uses actual `52FD90(this=755588)` and teardown `6D7DB0`.
No cap was reached or resumed. The initial copy-constructor hypothesis of blank
entries was rejected by actual bytes; only the virtual clone is blank.

## Actual renderer registration, not a callback substituted for engine logic

Original renderer vtable `6EFA40` slots 48/4C point to `4BB7D0`/`4BB840`.
Explicit renderer input provides only fields C9E8 and CA10..CA1C and an empty
circular list sentinel. It is not full renderer construction.

`4BB7D0` takes either `CA10++` or the first free index from list CA14, removing
the latter via original protected `4CDED0`. It calls device slot 11C with index
and 1024 entry bytes, writes palette.index, returns true **even on failed HRESULT**.
`4BB840` appends non-FFFFFFFF index to that free list (native `5956F0` allocation,
`4BB730` count update) but does **not** reset palette.index. A register/unregister/
reuse sequence proves index reuse and restored empty circular sentinel.
Repeated unregister/overflow and actual device palette selection remain open.

## Texture palette ownership hazards in the original

`4B93C0` is DX texture primary slot 3C. With no old palette it merely assigns.
With an old palette it invokes renderer unregister with the **new argument**, not
the old pointer, then directly deletes old palette and stores the new pointer.
An instruction observer at `4BB840` independently confirms that argument.

There is no same-pointer guard: assigning the same unregistered palette deletes
it and leaves the texture with a dangling pointer. No freed payload was dereferenced
in the bounded test. A NULL replacement when old is non-NULL would be dereferenced
by `4BB840`; this follows static instructions, not a completed NULL runtime test.

`4AB5D0` destroys DX texture by releasing device and COM texture then delegating
to `423220`/resource destruction. A loaded palette allocation and its registered
index remain alive; this was observed after actual deletion, not inferred merely
from a missing call. Explicit external cleanup occurs **after** this observation.
No claim that some untraced application shutdown path cannot clean global state.

## Runtime stream palette codec and failure returns

Original `4B2950`/`4B27C0` flat layout:
`u32 width,height,format; u8 palettePresent; [1024 bytes if exactly1]; u32 mipCount; packed mip rows`.
Raw ARGB and indexed one-pixel examples round-trip exactly through original
renderer registration and writer. A failing SetPaletteEntries HRESULT does not
change native success or serialized bytes.

| Declared failure | Actual result | Surviving native state |
|---|---|---|
| EOF at palette bytes | false, diagnostic | unattached palette allocation |
| CreateTexture returns failed HRESULT/NULL | **true**, no diagnostic | unattached palette; target stays empty |
| EOF at mip row | false, two diagnostics | unattached palette, temporary COM texture, locked/retained surface |
| Write palette bytes fails | false, diagnostic | partial header; attached palette/texture remain |
| Write mip bytes fails | **true**, diagnostic | partial output, locked/retained surface |

The last case is because `4B25A0` returns false but caller `4B27C0` ignores it.
Neither failed row helper unlocks/releases its surface on that error branch.
Tests explicitly unlock/release surviving fixture state only after recording the
returned native state. These observations are not successful export evidence.

## Reconstruction and safety policy

New `Code/Sparkplug/spPalette.h/.cpp` preserves original class identity, copying
distinction and index sentinel; paths/methods without symbols remain analytical.
Native uninitialized bytes are represented by host zero storage plus a validity
flag. They cannot be silently serialized as a valid all-black palette.
`spDXTextureSerializer` reads/writes the exact palette bytes using the same runtime
codec; CPU shadow leaves index unregistered rather than simulating a live GPU.

Host ownership uses `unique_ptr`, replacing and releasing palettes safely. It
deliberately does not reproduce native leaks, alias deletion or incorrect index
unregistration. Failed source rereads retain the prior valid palette; malformed
or uninitialized palettes fail before producing invented output. These are
explicit safety differences, not claims that original code had rollback/RAII.

## Validation and remaining scope

`pc-texture-palette-lifetime`: **17/17** bounded children, **114 native assertions**
(39 lifetime + 33 codec + 37 failure + 5 factory), **three exact compiled comparisons**.
Fresh source texture suite **813/813**; **32/32 CTests, 25.93 s**.
Largest palette lifetime arena 54832 bytes; clone 7826 instructions/2304 bytes;
palette codec at most 2000 instructions/53872 bytes. Limits stay100k/2 s per native
call,30 s per child,64 KiB arena/32 KiB native request; no GPU/OS forwarding/cap retry.
The first build rejected C++20 span in this C++17 project; pointer+size adaptation
fixed that compile issue without changing the native format or project standard.

Still open: full renderer lifetime/device selection/reset, real scene palette use,
all multi-resource/reinit failures, external sources, missing mip/conversion,
native-data writer integration, material/controller references and full FFPS Save.
Native-data writer reconnaissance separately completed three small originals;
its implementation/coverage belongs to the following checkpoint, not this one.
No PS2 credit, application or asset changes, commits, release or publication.
