# PC particle looping Init: directed counts 127/128/129

Three fresh original executions returned naturally from `48D1C0` through
`48D206` to the observer stop at `4B980D`. The exact 128 case emits no particles:
the original producer returns 128, calls sampler `48C100` with count zero,
and leaves all 128 records and the free pool unchanged. Preserve this original
behavior when reconstructing the producer; do not substitute a full last batch.

| Capacity | Sampler batches | Active/free after | Producer return | RNG draws | Final RNG index | Record bytes |
| ---: | --- | --- | ---: | ---: | ---: | ---: |
| 127 | 127 | 127/0 | 127 | 635 | 11 | 4064 |
| 128 | 0 | 0/128 | 128 | 0 | 625 | 4096 |
| 129 | 128, 1 | 129/0 | 129 | 645 | 21 | 4128 |

Recorded commands, from the repository root, executed serially and exited 0:
