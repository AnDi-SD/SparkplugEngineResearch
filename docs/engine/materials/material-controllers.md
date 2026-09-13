# PC material controllers and texture tracks — checkpoint 20

## Original classes, clocks and bindings

| Class | ID / registration | Original PC extent and main entries |
| --- | --- | --- |
| spRenderController | `14477AC7` / `75DEB0`, init `6D28BD` | `24`; accumulate `423190`, consume `4231A0`, copy `4231D0` |
| spAnimTexController | `16FB0E47` / `75D1E8`, init `6D1C00` | `4C`; factory `41A030`, update `42FC60`, delete `42FC40` |
| spTextureTrack | `0B3C1B09` / `760C38`, init `6D3EA0` | `20`; duration `478B40`, select `478C60` |
| spUVController | `1C0053D6` / `75D3C8`, init `6D1CF0` | `1FC`; factory `41A210`, bind `4346C0`, update `434820`, delete `4346A0` |

AnimTex stores a borrowed material-holder backlink at `24`, embedded
`spTextureTrack` at `28`, playback time at `48`. Embedded track has name at
`10`, times pointer `14`, intrusive texture-pointer array `18`, count `1C`.
Thus controller absolute offsets are times `3C`, textures `40`, count `44`.
Array storage has a count cookie immediately before its first pointer.

Material `476680` retains the controller and always updates its backlink,
including alias assignment. Two holders may retain one controller, but its
single backlink names the last holder. Direct NULL setter dereferences NULL;
the material reader guards NULL field 11 and leaves the old controller intact.

UV holds saved 3×3 matrix `28..4B`, transform evaluator `4C..1FB`. Binding
copies the holder's current matrix; rebinding to a different holder first
restores the old saved matrix on the former holder. Alias binding refreshes
the snapshot without restoring it. `467D90` sets all of texture-state word 8
to 2 (controller) / 0 (NULL), unless its bit `0x08` is already set, in which
case the word is preserved. Five UV and three Anim binding variants verify
these ownership/backlink rules, including last-owner destruction.

## Track read, frame selection and writing

Actual `43C470 → 43C2C0` reads field 0: count, all float times, then all common
texture references. Each frame slot retains its texture, including duplicates.
Whole directory loading is not part of the staged track comparison: objects
are initialized by the original tiny CPU cross reader, and already verified
native `466FA0` constructs explicit prebound FAT entries. Common resolver,
track reader, manager/index, and nested writer still execute original code.

`42FC60` consumes the accumulated delta, adds it to playback time `48`, then
subtracts duration repeatedly **only while playback > duration**. There is no
negative-time wrapping. Exact duration remains on the last frame; exact
multiple duration after subtraction also remains at duration, not zero.

`478C60` selects the first key whose time is **strictly greater** than playback,
except playback ≥ final time chooses the last key. These are interval end
times, not conventional start times. Duplicate times skip zero-length intervals.
The returned temporary texture owner is retained, installed as fallback and
released; each frame slot remains an independent retained edge.

Seven native/source comparisons cover three distinct textures, repeated same
texture, middle NULL, zero count, duplicate times, negative progression, and
float32 values just below/exactly at boundaries. All compare input bytes,
clock/selected-ID sequences and common serialized output, not merely counts.
Native writer uses field 0 `UInt32BeginEnd`, then count/all times/references.
Index recursively assigns controller ID 1 and unique textures IDs 2 onward.

Zero-count FileStream case is an observed failure: its zero-byte ReadData
returns false, the reader leaks its zero-size times allocation and exits before
the section terminator. The test records that failure, then explicitly frees
the leaked allocation during teardown. A separate writer invocation on the
still-empty object succeeds and emits `e0040000000000000000`. No reader retry.
MemoryStream's zero-length behavior is a distinct contract; portable codec
actually calls the stream for zero length, without hardcoding all streams to
fail. Host RAII does not reproduce the original allocation leak.

## Full small material → animation → runtime texture graph

Four cases: inline, repeated controller, NULL-after-controller, two layers
sharing one controller. Three frame slots all alias one tiny runtime DXTexture.
The explicit balanced three-node RTTI tree contains actual StdLayer, AnimTex,
DXTexture records/factories. The tiny COM device stores one 1×1 format-3 mip;
no real renderer/OS/GPU startup or API forwarding occurs.

The complete native material reader succeeds, the material render-update entry `467B70` invokes the controller, and the same shared core saves the graph. When two layers share the controller, updating the first holder changes the **last-bound second holder**. Writer emits one canonical texture, referenced by both fallback and frame slots, with native traversal-dependent ID order.

Maximum observed graph read: 52,674 instructions. Maximum arena: 59,040 bytes.
All graph/manager allocations are released; device baseline 1, texture/surface
refs 0, no locked surface. Caps unchanged: 100k instructions / 2s call,
64 KiB arena / 32 KiB request, 30s fresh child.

## Reconstruction and intentional safety boundaries

AnimTex serializer is upgraded from a grammar planner to actual shared-core read/index/write. Material field 11 now carries a canonical shared controller owner; NULL preserves it. A whole enclosing field cannot be passed as if it were one reference; the first source comparison exposed and corrected that mistaken guard usage.

Host contexts pin objects; controllers detach stale material backlinks on
holder destruction/replacement, and permit explicit safe NULL clear. Native
backlinks do not have those guards. Runtime rejects unbound, nonfinite,
unsorted, zero/negative-duration and excessive-wrap inputs; raw serialized
times remain preservable. Normal verified finite sequences keep native rules.
Controller/texture-track graph cloning remains refused, not silently shallow
copied. Static inspection of `42FE6B` suggests raw copied texture-pointer slots
without visible retains; full clone ownership is still a follow-up question,
not a reproduced safe native clone claim.

## Remaining connected work

UV transform/function evaluator math and nonempty UV codec, color-controller
counterpart, native controller/track clone ownership, malformed/partial failure
paths and native track release/reload variants, exact standalone track
factory/copy, complete renderer state application, real-file/native FFPS save
and live PC display remain open. Original API/source names remain separate
name-only questions. No PS2 credit or 100% completion is inferred here.
