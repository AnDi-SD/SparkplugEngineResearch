# PC spDXLight — CP44

PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Original class6B3E7BAA, direct base spLight72444900, record7634B0,
initializer6D4EC0, factory4AC000, ctor4B5370. No original TU/header path
survives here; `Code/SparkplugDX/spDXLight.*` is explicitly inferred.

## Construction, copy and lifetime

Actual factory allocates158 bytes. Primary6F0C88 has15 slots:
4B58B0,420B40,4AC240,4B5910,4B5390,408350,408370,420E30,420E60,
4212F0,420610,421330,4B58D0,421640,428DD0. SecondaryB4=6F0C84
routes through4B5960 to4B58D0 after subtractingB4. AddedF0..157 (26words)
are entirely untouched allocator bytes, not an initialized device light.
Destructor4B53A0 restores both vptrs then428DB0; no owned device object/COM.

Copy4B5910 calls actual inherited428EB0, then copies all26 device words.
Like the base, it **does not copy intensityD8**. Existing target9.5 survives
source3.25; actual allocating clone4AC240 registers with real CloneManager,
copies, and keeps constructor intensity1.0 while copying old device payload.
That potentially stale payload/intensity combination is preserved as evidence.
No fake clone-pair seam was used in these new bounded leaf-root transactions.

## Device payload producer4B53C0

Every call writes rangeE0→13C and falloff140=1 before type dispatch.
Types beyond0..2, including ambient3, then return leaving all other words
untouched. Known device fields, relative to complete light:

|Offset|Meaning in device payload|
|---|---|
|F0|type: engine0/1/2 → device3/1/2|
|F4/104/114|diffuse/specular/ambient float4|
|124/130|position/direction float3|
|13C/140|range/falloff|
|144/148/14C|constant/linear/quadratic attenuation|
|150/154|theta/phi|

Directional0 uses world directionA4 and default vector7600E0 as position;
point1 uses world position74 and that global vector as direction; spot2 uses
both world inputs. The globals are explicit test inputs, not claimed startup
defaults. Ambient for all three is ARGB73FE98→RGBA via actual424700.

Directional diffuse/specular = color * intensity. RGB is capped **only above1**,
negative values survive; alpha is multiplied but never clamped. Attenuation
is1/0/0; theta/phi are untouched. Point/spot diffuse/specular copy color
without multiplying intensity. If attenuation is enabled and range/intensity
both>0, constant=1/intensity and linear=float32(0.7F /
((wide(intensity)*range)*0.3F)), quadratic0. Spot fallback is1/0/0, but point
fallback is **1/intensity,0,0 even when disabled or intensity zero**: original
zero-intensity point produces+Inf. Point theta/phi0; spot copies E4/E8.

Source `RefreshDevicePayloadForAnalysis` reuses common ARGB math, keeps
unknown/unwritten words as optionals (not zeros), and accepts finite explicit
world/color inputs. Negative/zero intensity/range are tested; derived point
Inf is preserved. No exhaustive NaN/Inf/x87 equivalence is claimed.

## World callback boundary

4B58D0 snapshots storedB0|inherited flags; bit1 implies bit8. It calls actual
428C30→421420, then if enabledED and the captured bit8 is set, calls4B53C0.
428C30 clears local bit8 and may refresh Scene's LightManager if scene3C exists.
The native test executes complete no-scene world callback with five flag/
enable inputs. Source compares the predicate and payload using declared
already-updated world inputs; **full source scene/world callback wiring is
still incomplete**. Native uninitialized startup matrix globals are not
silently treated as source constructor defaults.

## Verification and open work

pc-dx-light8/8 exact groups,47 counted native checks; source37/37,
build261/261 (shared clone friend required rebuild), CTest47/47 in48.70s.
Report `local-data/results/bounded-native-runs/20260907T031045973479Z-pc-dx-light.json`.
Maximum43,469 instructions/928 arena bytes for these standalone probes;
all original100k/2s/64KiB/32KiB/30s caps unchanged. No GPU/game run.

Open: shared scene/world integration, light setters/dirty propagation,
controller-driven changes, real startup globals, inherited complex graph
clone, serializer-manager runtime creation wiring and full renderer upload
from real loaded SMO. Source paths/member names remain partly inferred.
PC-specific class work adds no PS2 credit and closes no completion gate.
