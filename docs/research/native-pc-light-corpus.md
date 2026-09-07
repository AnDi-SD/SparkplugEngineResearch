# PC real SMO light sections and matrix-to-quaternion store precision

CP89,2026-09-07. Same pinned executable and unchanged100k/2s/64KiB/32KiB/
30s caps.19 exact captures/92 native assertions:9 real sections72 and10
standalone matrix conversions20. Every actual codec owner is destroyed.

`research/pc-light-corpus-fixtures.json` pins whole-file hashes,exact offsets,
sizes and slice hashes for nine distinct compact field shapes in pristine
PC corpus_id2. The database also contains a working corpus; its entries
were excluded after a whole-file hash mismatch before any guest execution.
The nine cases represent shapes observed across512 compact LightData rows,
not512 executed objects. A large350044-byte child-bearing shape is excluded.
No original asset bytes were edited. Selected sections are19..57 bytes plus
one22-byte ambient variant; actual sizes are recorded in the fixture file.

Original Data header4400B0 creates DXLight; whole440640 calls Node463A70 and
virtual4B58D0 before reading own fields. Writer440110 includes inherited
Node orientation conversion,then an actual fresh DXLight rereads the output.
Captures include all light scalars/flags/opaqueDC and30 local/world Node
float words,complete output and the fresh object's state. Reader8750..13220,
writer5053..7772 instructions;876 requested engine bytes,all owners freed.
Scene membership and parent transforms remain absent explicit inputs.

## Shared source bug exposed by real asset

Bloom_body.smo's Light orientation reads identically, but the prior source
writer produced quaternion w=3F3504F3 versus native3F3504F4. Native464CB0
keeps trace,fsqrt and reciprocal on the x87 stack until component stores;
the source incorrectly narrowed intermediates to float. FromMatrix now
retains double intermediates in both positive-trace and maximum-diagonal
branches. The actual corpus case becomes x=3F3504F2,w=3F3504F4, including
the ensuing fresh read. Ten independent finite matrices cover each major
diagonal,exact ties,small positive trace and the real orientation. Whole
native helper30..84 instructions,64 raw arena bytes,no seams.

This is exact selected finite evidence,not universal double/x87 equivalence
for all exceptional matrices. The source does not normalize the quaternion.

Reports in `local-data/results/bounded-native-runs/`:

- `20260907T135451744761Z-pc-light-corpus.json`:9/9 exact.
- `20260907T135451973769Z-pc-matrix-quaternion.json`:10/10 exact.
- `20260907T135452255556Z-pc-node-output.json`:11/11 existing regression.
- `20260907T135452479197Z-pc-skin-light-constants.json`:10/10 regression,
  retaining real SAN/scene/mesh/Skin full constant/draw composition.

The first pre-fix corpus profile stopped at the observed mismatch and is
preserved; it is not counted as another successful capture. Full-file SMO
loading,512 object execution and live lighting/GPU are not claimed.

Full build/CTest61/61 passed in45.43s after the CP89 math correction.
