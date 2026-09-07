# PC Skin: actual light world to shader constants

CP87,2026-09-07. Pinned original WinxClub.exe SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
10 exact captures,796 native assertions (706 linked and90 shared draw).
Limits remain100k instructions/2s per original call,30s child,64KiB arena,
32KiB allocation request. All102 engine owner generations are released;
peak64488 bytes,read70998,render15893..16175 instructions.

The same actual DXLight constructed by4AC000 now receives local position
and orientation followed by the whole base Node421420 world producer.
World position74=(4,5,6),world directionA4=(1,2,3),local direction58 are
captured as raw words. This is explicitly the base Node method followed by
DXLight4B53C0 payload generation, not the Light virtual world/SceneManager
selection lifecycle. Renderer viewCA80 is an explicit nonidentity input.

Actual4AF940/45A6C0 append four descriptors after BlendMatrices0..2 and
MatDiffuse3. Original46A240/4BC4A0/4AE930 uploads all eight complete rows.
Ordinary names are LightMatDiff,LightPos,LightDir,LightAttenuation; special
names are AmbientCol,LightAmbientColorDir0,LightDiffuseColorDir0,
LightSpecularColorDir0. Cases cover directional/point/spot/disabled,
negative device HRESULT,false post, and directional/disabled/ambient/empty
special groups. Entire SAN,scene,mesh,material,light state and draw traces
remain bitwise equal to source, not just the added constants.

LightMatDiff uses raw object colorC4 rather than intensity-scaled device
payload. Directional position is negative transformed direction times1e9;
point/spot position includes view translation. LightDir has w0. Attenuation
comes from actual DXLight payload144/148/14C plus rangeE0. Disabled ordinary
lights remain in ordinary constants but are absent from the enabled Dir0
group; special material specular behavior retains the existing codec.

Source whole geometry submission now resolves constant inputs from the
same spDXLight objects used by the device light list. It connects actual
world/local getters,payload attenuation,renderer ambient/view and selected
group identity. Missing source object or unavailable attenuation are host
guards. NULL-list stale directional constants, arbitrary IEEE/x87 inputs,
first lit shader generation,whole Scene frame and live GPU remain open.

Validation: `20260907T132250062506Z-pc-skin-light-constants.json`10/10;
CP86 regression `20260907T132300855979Z-pc-skin-lights-render.json`16/16.
Full build and CTest60/60 passed in52.80s after the final CP86 C194 bridge
and all CP87 source changes. Reports are under
`local-data/results/bounded-native-runs/`. Class scores are retained.
