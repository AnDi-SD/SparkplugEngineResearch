# PC material → texture: common reference graph (checkpoint 19)

Evidence: original `WinxClub.exe` SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PC only; completed during the bounded cycle ending 2026-09-07 07:30 MSK.
This extends [standard material graph](native-pc-material-standard-graph.md)
and [runtime texture codec](native-pc-texture-runtime-mips.md).

## Confirmed original execution

`41A390` creates the CPU material and `42F690` its serializer. Reading the
material uses `42F670 → 4774D0`. Field 10 invokes common resolver `4678B0`
with expected class `spTexture` (`2F281E13`); an inline runtime texture
`3F3651B6` is constructed by `4AB520` and read by `4B2950`.
The expected-class argument alone is not a native safety check: the common
resolver's previously documented behavior remains unchanged.

The fixture supplies a minimal valid RTTI search tree and factory records,
an empty manager/FAT, platform 2 / operation 1 / policy 2, and an explicit tiny
COM storage device. This is not cold-start registration or GPU evidence.
The texture is 1×1, format 3, palette-free, with one four-byte mip and
eight-byte physical row pitch. Input record is class ID + `SBOO` + flat body.

After read, clear the non-owning FAT, switch operation to 2, then call
`4672C0` to index recursively: material ID 1, texture ID 2. The original writer
emits field 10 with `UInt32BeginEnd`, invoking the same common writer core.
A second reference is ID 2 / size 0, not a duplicate texture body.

| Fixture | Native assertions | Result |
|---|---:|---|
| inline | 11 | one canonical texture and exact output |
| repeat | 11 | repeated field points to same object |
| two-layers | 11 | both holders share one texture; refcount 2 |
| null-after | 10 | NULL clears fallback; no texture in output |
| orphan | 7 | field without a layer is skipped, no texture constructed |

Highest measured read: 40,057 instructions; highest arena position: 58,368
bytes. Every native allocation and texture/surface reference is released at
teardown; device baseline reference remains 1, and no surface remains locked.
Caps stay 100,000 instructions / 2 seconds per call, 64 KiB arena, 32 KiB
allocation request, 30 seconds per child. No original OS/GPU calls forwarded.

## Ownership and a rejected NULL hypothesis

`41E870` compares old/new fallback pointers. Alias is a no-op; otherwise
release old (delete on last intrusive reference), retain new and store it.
`467CF0` releases animation controller, UV controller, then fallback texture.
The separate ownership probes cover alias, rebind, two holders and clone:
5 + 5 + 5 + 6 = **21 assertions**. Clone uses the actual map initialization
`52FD90(this=755588)` and cleanup `6D7DB0`; static matrix/state are copied,
the fallback is shared/retained, not cloned. Nonempty controller clone graphs
remain open. These tests also free the clone map and all objects.

Initially the test expected NULL field 10 to preserve an existing fallback.
The original disproved that: `477944 → 41E870` is unconditional. The fallback
is deleted if it loses its last reference, but the FAT retains a stale address.
The corrected test does not re-resolve or dereference that freed native object.
This was an assertion/hypothesis correction, not a capped-probe retry.

Fields 11/12 differ: their NULL resolver result skips the setter. Also the
earlier PC controller labels were reversed. The verified PC layout is:

| PC material texture offset | Role | Reader/setter |
|---|---|---|
| `34` | fallback `spTexture` | field 10 / `41E870` |
| `38` | `spUVController` (`1C0053D6`) | field 12 / `467D90` |
| `3C..5F` | nine-float UV matrix | field 9 |
| `60` | static UV enable byte | field 9 |
| `64` | `spAnimTexController` (`16FB0E47`) | field 11 / `476680` |

PC ABI assertions now include both controller offsets. No automatic PS2
layout correction or research credit is inferred from these PC observations.

## Reconstructed source and verification

`spMaterialSerializer` now reads, indexes and writes nonempty texture links
through the existing shared `spSerializer` reference core. It retrieves the
canonical `shared_ptr<spTexture>` from the read context and installs it on the
holder. Two layers share the same owner. Field 10 NULL clears the holder.
The host context deliberately pins created objects, avoiding the original
stale FAT lifetime; this safety adaptation is not native deletion equivalence.

`spMaterialTexture` offers explicit owned fallback access alongside the old,
explicitly borrowed raw-pointer setter for analysis fixtures. Borrowed pointers
are not accepted as a serializable owned graph. Alias raw assignment preserves
an existing canonical owner. Clone shares that owner and copies static state;
it refuses nonempty unknown controller graphs instead of shallow-copying them.

Commands:

```text
python research/native_workbench.py run pc-material-texture-links --platform pc
SparkplugMaterialSerializationTests.exe --material-texture inline
```

Profile: **9/9** fresh bounded children, **71 native assertions**, **5/5 exact
native/source input, alias topology and output comparisons**.
Rebuilt material tests: **484/484**. Whole source CTest: **32/32**, 20.13 s.
The follow-up build removes a local shadow warning and adds the UV ABI assert;
it does not change the graph algorithm.

## Still open

Nonempty UV/animation/color controller graphs and evaluators, their clone,
update/time/selection rules, shader/custom layers, malformed/partial allocation
and write failure paths, lossless unknown fields, full FFPS save and live PC
display remain required. The earlier whole-logo material allocation cap stays
disabled. This checkpoint proves a small shared graph, not arbitrary real-file
end-to-end readiness. Next: controller bindings and the update consumers of
the exact `38/64` edges above, with independently bounded probes.
