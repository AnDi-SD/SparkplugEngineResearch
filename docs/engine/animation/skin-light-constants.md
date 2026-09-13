# PC Skin: actual light world to shader constants

The same actual DXLight constructed by4AC000 now receives local position and orientation followed by the whole base Node421420 world producer. This is explicitly the base Node method followed by DXLight4B53C0 payload generation, not the Light virtual world/SceneManager selection lifecycle. Renderer viewCA80 is an explicit nonidentity input.

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
