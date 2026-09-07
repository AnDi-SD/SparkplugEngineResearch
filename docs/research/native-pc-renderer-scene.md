# PC scene boundaries and real material payload — CP27

Pristine PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original internal wrappers execute; only external COM methods are observers.
This is not GPU rendering, renderer startup, or protected-controller creation.

## Begin/EndScene

PC renderer secondary interface `complete+18`, slots3/4:

- **4BB950** calls device `+A4` first, then writes complete `+CBC0=1` and
  `+4C=0`; returns AL1 regardless of HRESULT. Does not increment counter40.
- **4BB9D0** calls device `+A8` first, then increments complete `+40` modulo
  uint32 and clears `+CBC0`; returns AL1 regardless of HRESULT. Does not reset4C.

Neither wrapper checks whether a scene is already active. Duplicate Begin
or End still invokes COM; duplicate End increments twice. A declared nonboolean
initial active byte7 is normalized only by the relevant post-call store. COM
observers record the **old state**, proving call/cache ordering.

Counter40 is the same renderer field used by DXMaterial4A9530 in CP26. A
combined test invokes material update around scene calls, with actual color
controller runtime and explicit .25 deltas. It checks propagation of counter
changes, not a claim that this synthetic call schedule is the game's full
frame schedule. The separate app/core ordering remains in CP5's frame dossier.

Portable `spDXRenderer::SceneStateForAnalysis` exposes just these three known
fields to static begin/end methods; it does not invent the full native class
layout or constructor defaults. `resetWord4C` keeps an offset-based name because
its broader counter meaning/original spelling is not proved. NULL device callback
is a host-only rejection guard; HRESULT handling follows the actual wrapper.

Declared native renderer backingCBC4 is under64KiB; actual native allocation
cap remains32KiB. Max arena53,392 bytes in combined case,52,464 standalone;
max143 original instructions per wrapper/material call, no GPU forwarding.
The material controller retains a declared external input pin to avoid its
unproven destructor. All actual tracked allocations are freed.

`pc-renderer-scene`: **8/8 exact captures,35 native assertions**.
`SparkplugRendererSceneTests`: **27/27**. Cases begin/end, duplicates, HRESULT
failure, counter wrap and material propagation. Build48/48; final CTest38/38.

## Read-only real PC ColorController sample

Corpus DB identifies `Characters/Knut/lightbeam_projectile.smo`, object5,
physical offset564, serialized size54. Pristine sample has5584 bytes and SHA256
`7BEA3AF6DC3643BBA1CED3E72B61743EB12F77EE9ACCDCC6C86D092707D53482`.
Header is original4C633E85/SBOO, followed by46-byte payload (field0 length40).
Payload SHA256 `823326E30F0176C775FBA24C457DC0079EA59ECB751DC5D9C4637C29816A4C13`.

`probe_pc_material_color_corpus.py` reads the fixed original sample and checks
its hash/identity/size. Actual4412E0 and441740 read/write it on declared target
state, with4373E0 updates in between. Output is byte-identical. Four color
frequencies=.1; alpha frequency=.1, amplitude0, Y offset1; types absent in wire.
With **explicit type0 input**, all five evaluations are skipped and an existing
diffuse alpha.375 is preserved. This does not prove the capped constructor's
actual defaults or that every game instance is a no-op.

Common source decoder/encoder and runtime match the actual original capture.
`pc-material-color-corpus`: **11 native assertions,1 exact capture**;
MaterialColor source suite now**225/225**. Max native13,085 instructions,
arena1,472 bytes. No asset files modified. Build2/2 after corpus test addition;
full CTest**38/38**,27.41 seconds.

## Reproducibility / remaining boundary

Runner report schema2 freezes the startup configuration's canonical JSON hash.
Schema1's current-file hash could reflect work-queue edits during a run; two
early records remain historical and are not silently rewritten. Reports do not
capture all transitive source/binary hashes and are not research-completion
scores. Runner tests13/13.

Evidence: native/compare `pc_renderer_scene` and `pc_material_color_corpus`
scripts, `Sparkplug/Tests/spRendererSceneTests.cpp`, common source code. No PS2
score transferred, no class100/gate passed. Next connected backend boundary:
Clear4BB980 and Present4BB9F0; reset/device-loss and whole renderer remain open.
