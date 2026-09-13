# PC decoded light: complete no-scene virtual world callback

Modes directional,point,spot,disabled,ambient and parent cover initial
dirty refresh,raw intensity change without dirty marker,explicitdirty8,
inherited8,and disabled local position change. Raw change leaves device
payload stale; a later marker updates it. An actual Node owns the parent
case light through421A60; whole parent421420 dispatches child4B58D0 and
produces final world position(17,-12,39) after local(7,8,9). Source uses a
canonical shared child owner and the same parent world traversal.

Scene3C remainsNULL. Scene LightManager46ACE0 refresh,registration,renderer
light-list selection and finite-failure propagation are separate boundaries.
No native void-return is invented as a boolean; source bool is a host guard.
Full Scene/GPU/PS2 behavior is not claimed.
