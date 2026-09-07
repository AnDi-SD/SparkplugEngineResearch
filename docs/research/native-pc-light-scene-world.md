# PC decoded light: whole Scene attachment and world cache refresh

CP92,2026-09-07.6 exact captures/186 native assertions. Actual45EBC0 Scene
and its four owned managers/root, two425520 RenderNodes, and4400B0/440640
decoded DXLight are retained together. Typed421A60 attachment publishes the
actual scene3C and intrusive LightManager membership. Whole4B58D0 reaches
428C30→421420→46ACE0/46AC60→46A850→490B50/490BA0, then device4B53C0.
World2379..2720 instructions,54304 arena bytes,1989 requested engine bytes;
actual scene destruction frees light and both nodes plus every helper owner.
Pinned original EXE and100k/2s/64KiB/32KiB request/30s child caps unchanged.

Each directional/point/spot/disabled/ambient/shadow case captures the attached
state and nine successive world operations: initial, far position, dirty8,
disabled, enabled, inactive100, active100, changed target sphere, inherited8.
Two actual target nodes have explicit current world spheres at0 and100;
the second sphere later moves to1000. Shadow exclusion differs between them.
Snapshots include all14 Light words,120 Node bytes,26 device words and both
complete raw light caches, including unused slots and ambient/count.

Position-only dirty1 refreshes the DX device payload but does NOT refresh
scene selection:428C30 tests post-Node flags|inherited8, after Node cleared1.
An explicit later dirty8 removes out-of-range point/spot lights. Disabled
and inactive lights are removed even though their device refresh differs.
Ambient uses its separate cache slot; changing sphere is read on next refresh.
The first ordinary refresh resolves protected cache add and has the largest
instruction count. No equality assertion treats that startup cost as behavior.

Source spLight now holds an explicit borrowed SceneLightManager binding.
Its world clears8 and invokes that manager before the derived DX payload.
spLightManager retains explicit borrowed cache/sphere targets and applies the
same eligibility/add/remove algorithm in stable order. Register/unregister
guards and vector storage are host choices. Binding is not automatic native
Node attachment, a complete source Scene class, partition traversal or scene
render initialization. The native manager+1C render-list link is the explicit
45D850 assignment; the unrelated full SceneInit is not executed. Renderer
storage is zero caller input inside the same arena, without OS/GPU forwarding.

Final report20260907T144133264994Z-pc-light-scene-world.json6/6.
CP91 regression20260907T144133291174Z-pc-skin-decoded-light.json11/11.
Full build/CTest61/61 passed after CP92 source changes in47.83s.
Cumulative CP51..92:325 exact/9629 native assertions, plus38 native-only CP88.
