# PC decoded light: complete no-scene virtual world callback

CP90,2026-09-07.6 exact captures/86 native assertions; each captures decoded
state and five successive whole world operations. Original pinned EXE and
100k/2s/64KiB/32KiB/30s caps unchanged. Read11411 instructions,world115..513,
424 requested engine bytes standalone,676 with actual parent. All engine
owners and late-created SceneManager are destroyed by actual destructors.

Actual4400B0/440640 reads a DXLight including Node position/quaternion and
own type,color,attenuation,intensity,range,angles,enabled. The Node section
already calls virtual4B58D0 and4B53C0 with constructor light fields, before
the own section mutates them and sets dirty8. Capture retains that initial
payload rather than inventing a clean/zero cache after deserialization.

4B58D0 captures storedB0|inherited; bit1 implies refreshbit8. It calls
428C30→421420, then checks enabledED and capturedbit8 before4B53C0. Base
428C30 clears ownbit8 after Node clears1..4. Enabled is read after base
propagation; disabled lights still clear flags and update world transforms.
The source Light and DXLight now override the same world method, using the
same object local/world fields and actual common device payload producer.
Default-vector7600E0 and ambient73FE98 are explicit host world inputs.

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

Reports under `local-data/results/bounded-native-runs/`:
`20260907T140506324825Z-pc-light-world.json`6/6;
`20260907T140353517760Z-pc-light-corpus.json`9/9 regression;
`20260907T140353470129Z-pc-skin-light-constants.json`10/10 regression.
Full build and CTest61/61 passed in43.60s after all CP90 production changes.
