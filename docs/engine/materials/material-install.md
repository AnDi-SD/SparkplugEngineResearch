# PC material installation and state batch

## Installation before animated color update

4BE180 stores a **borrowed** material pointer atE47C, copies17 words from
material+78 to renderer+E4A4, then calls material's adjusted+14 interface
slot0 with force0 (`4A9530`). It always returnsAL1; no COM call here.
Thus the installed color block is the material's **pre-update** state.
This ordering is verified on none/animated/shared controllers, same-frame
repetition and consumed clocks. It is not silently changed to update first.
The shared controller still updates only its last-bound material.

## Material states and override table

4BB890 refreshes `C1C8 = C18C+18` (selected material's state array), then
unconditionally sets raw lighting cacheC888 to255. It visits indices1..10
in order, calling actual primary+78/4B0AD0 only for unequal raw values.
Lighting therefore normally reexecutes every application, although lower
device-state cache may emit no commands. Raw index0 is not applied.

With byteC1C4 clear, desired state is `C1C8[index]`. Otherwise selector at
`C71C+4*index` picks a source pointer from `C1C8+4*selector`; the same state
index is read from that selected array. Slot0 is freshly replaced by the
current material state array. Source pointers/selectors are caller input,
not guessed defaults or a guessed maximum native array extent.

Source `ApplyMaterialStateSetForAnalysis` preserves forced lighting, index
ordering and per-state overrides, using common restored state translation,
lighting and lower device cache. Native failed HRESULT remains ignored.
Invalid source slots/NULL/malformed table values are explicit host guards,
not claimed safe native branches. No native error branch was faked.
