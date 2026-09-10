# PC particle looping Init: directed counts 127/128/129

Three fresh original executions returned naturally from `48D1C0` through
`48D206` to the observer stop at `4B980D`. The exact 128 case emits no particles:
the original producer returns 128, calls sampler `48C100` with count zero,
and leaves all 128 records and the free pool unchanged. Preserve this original
behavior when reconstructing the producer; do not substitute a full last batch.

This is a directed follow-up to the immutable
[real PC2 bg.smo proof](native-pc-particle-loop-init-pc2-2026-09-10.md).
The real file remains unchanged (279915 bytes, SHA256
`5F0B9FFA6EFF16C278E4701EBE0A2518C7496189A036BFCB14519E2B5CD59BA5`).
Each separate fixture copy changes only two float32 payloads: lifetime at
absolute offset 17136 from `6666e63f` to `0000803f` (1), and rate at 17141
from `00009643` (300) to 127, 128 or 129. All other bytes, including references,
remain identical. These are explicitly supplied inputs, not real corpus counts.

| Capacity | Sampler batches | Active/free after | Producer return | RNG draws | Final RNG index | Record bytes |
|---:|---|---|---:|---:|---:|---:|
| 127 | 127 | 127/0 | 127 | 635 | 11 | 4064 |
| 128 | 0 | 0/128 | 128 | 0 | 625 | 4096 |
| 129 | 128, 1 | 129/0 | 129 | 645 | 21 | 4128 |

All three consume the particle serializer through cursor 17198 and retain the
actual RenderNode world transform (including its rotation). Pool links remain
reciprocal, unique and cover every record slot. Before Init every pool is free.
At 128, the complete record storage and PRNG state remain unchanged; untouched
record bytes include allocator poison, which is not an engine default value.
At 127/129 native `413270` is visited once by the first `4132B0` draw. The harness
does not seed the PRNG: mapped state index 625 is a declared fixture input and
does not establish the game startup state.

Each fresh child uses the existing character profile (4M instructions, 16 s),
256 KiB arena, 32 KiB individual allocation cap, 30 s external child cap and
one worker. Loader execution uses respectively 1,747,417 / 1,660,373 / 1,749,070
instructions and 7.032 / 5.063 / 5.379 s. No production source or build changes
were made for these probes.

Recorded commands, from the repository root, executed serially and exited 0:

```powershell
python local-data/results/tools-core-cycle-20260910-0730/particle-loop-init/pc2-bg-count-boundaries-probe.py 127
python local-data/results/tools-core-cycle-20260910-0730/particle-loop-init/pc2-bg-count-boundaries-probe.py 128
python local-data/results/tools-core-cycle-20260910-0730/particle-loop-init/pc2-bg-count-boundaries-probe.py 129
```

The [manifest](../../research/native-pc-particle-loop-init-counts-2026-09-10.json)
fingerprints each report, exact script snapshot and input copy. Captures precede
teardown and the remaining scene load: whole-file acceptance, teardown and
reconstructed-source equality are not claimed. Only 127/128/129 were executed;
other exact multiples such as 256 remain unexecuted. Zero direction, other
world-space modes and special angle/basis branches still need their own evidence.
The earlier 539-particle real-file proof remains unchanged. Its truncating count
calculation already agrees with the shared code's double product of float inputs.
