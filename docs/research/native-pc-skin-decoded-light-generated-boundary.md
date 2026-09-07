# Decoded light first-generation bounded candidate

Historical 64 KiB experiment. A subsequent explicit 128 KiB profile completed
two original/source comparisons (directional and ambient), with peaks
65,960/65,944 bytes; see the [post-cycle experiment](native-research-strategy-2026-09-07.md).
That result does not change the failed 64 KiB captures below.

2026-09-07 after CP91; no exact capture or coverage credit. Candidate scripts
probe/compare_pc_skin_decoded_light_generated.py retain the same real SMO
light and real SAN bone through decoded mesh/material and whole46A240.
The external compiler reflection declares BlendMatrices/MatDiffuse and four
ordinary or special light parameters, eight rows. The shader template still
has empty code and Main/vs_2_0. This is not valid shader/GPU evidence.

The current final fixture reads the light after the Skin material, before
SAN and renderer setup. Its original light object survives all later phases.
The complete full-size renderer, two-layer material and 64KiB cap remain.
It has not completed first generation; no capped call was resumed or replayed
with unchanged input/placement. Failed placement experiments are not tests
of the executable's own allocator strategy; allocator placement is external.

Observed fresh variants, all stop without fabricating internal success:
- Original late-light/best-fit directional:288-byte string allocation at
  40F83E→4123F0,65200 reserved,largest free280.
- High caller list and then high palette: same288 request; holes respectively
  280/8/16/32 and280/8/48. Late ambient reached32 at65504,holes8/8/16.
- Fresh first-fit render phase: directional288/ambient272 at65200,
  holes128/8/32/80/88.
- A distinct one-layer wire specimen saved one Std/Texture owner but exposed
  an earlier original fault:4BBCB0 selects fallback pass layer1 for unused
  stages,4BBCB4 readsNULL+10. The candidate/source helper were restored to
  the declared two-layer wire; this invalid draw input earns no exact credit.
- Early light with best-fit:353 request at64928,holes320/16/16/48/16/160/32.
- Early light with first-fit reader:353 at64928,holes8/224/168/16/192.
- Fresh all-owned-high placement failed before rendering:62308-byte renderer
  backing request at1416 reserved,largest hole59944. The exploratory allocator
  option was removed; no live memory was moved, overlaid or silently expanded.

Logs remain under .codex-tmp/decoded-light-generation-*.log. The successful
CP91 cached-shader chain and the earlier unlit first-generation chains remain
independent evidence. Next useful work requires a proven lifetime/layout
change or a smaller independently specified chain, not retrying these frames.
