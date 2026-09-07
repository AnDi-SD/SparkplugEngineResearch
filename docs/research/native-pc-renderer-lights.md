# PC renderer light submission — CP43

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual4BDE50 runs completely; only external COMCC/D4/E4 are observers.
Three declared light records contain explicit26-word device payloadsF0..157;
their producer and live GPU are not silently reconstructed by this test.

Non-NULL light cache has ordinary pointers0..1C, ambient pointer20, count24.
The method clears24 pointer slots at rendererF2F8, then for each input light:
if enabled byteED is nonzero, submits COMCC(index,light+F0), enables COMD4,
then appends its identity to typeC0's eight-slot group. Disabled lights only
receive COMD4(false), and do not enter the grouped cache. All HRESULTs ignored.
It disables leftover slots up to previous count in process-global764340,
then replaces that global count. Repeated active lights are always submitted.

An enabled ambient pointer supplies colorC4..D0; otherwise73FE98 ARGB is
converted using exact6DCA9C coefficient. Each RGBA difference is compared
in widened arithmetic to float32 epsilon6F1DA0=`3A83126F` (~0.001).
If no channel exceeds it, both float ambientC178 and device state139 stay
unchanged. Otherwise float cache updates first, channels are multiplied255
and truncated through actual60DB90, low bytes formARGB, and common4B0A90
deduplicates/caches state139 after COM. Tests include sub/super-threshold
changes, repeats and disabled ambient switching to global fallback.

Crucial NULL distinction: disable all8 slots, zero ambient, apply state139=0,
then clear borrowedC190. **Grouped pointers and global previous count are
not cleared.** Non-NULL calls, including empty lists, never assign C190;
the caller normally already installed that list. An empty non-NULL list does
clear groups and count. Source preserves these observed asymmetries.

`SubmitLightsForAnalysis` reproduces this using explicit input/state structs.
Finite colors, abs(color)<=4096, ordinary types0..2/count<=8 and callbacks
are host guards, not new format restrictions. Native malformed lists/types,
NaN handling, payload production, scene/world invalidation, lit full draw,
multi-renderer global ownership and live backend remain open. No PS2 credit.

Profile6/6 exact,30 counted native; source24/24,build61/61,
CTest46/46 in31.51s. Report:
`local-data/results/bounded-native-runs/20260907T025453167009Z-pc-renderer-lights.json`.
Maximum347 instructions/63,952 arena; original100k/2s/64KiB/32KiB/30s caps
unchanged. The connected next class is originalspDXLight, which produces
the consumed26-word payload; a separate factory scout confirms size158.
