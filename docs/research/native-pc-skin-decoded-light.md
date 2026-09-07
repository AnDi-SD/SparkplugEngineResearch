# PC unchanged SMO light through complete lit Skin draw

CP91,2026-09-07.11 exact captures/921 native assertions: nine pinned compact
SMO LightData sections from CP89, plus failed-device and post-false for case6.
The same decoded DXLight survives header/read, virtual world, eight shader
constant rows, whole SAN/scene/mesh/own-material Skin draw, and actual destruction.
ReadSkin70998,LightRead2398..5109,LightWorld92..184,render15935..16175
instructions; peak64624/65536 bytes,105 engine owners all freed. Original
EXE hash and100k/2s/32KiB allocation/30s child limits remain unchanged.

Actual43FFD0/4400B0/440640 consumes each unchanged, SHA-verified SMO slice.
Its decoded state includes14 light words and120 Node bytes. Whole4B58D0
then428C30/421420/4B53C0 operates on that exact object; whole46A240 selects
the same decoded material and mesh, uploads light state and all eight rows,
and calls4BE210. Real bbush.san has already updated the retained bone through
actual Actor/AnimManager and SceneManager world traversal. Three ambient,
three point and three directional compact fixtures are covered; spot is
separately covered by CP90, not represented by these nine shipped slices.

The first ambient fixture exposed a source constructor omission. Original
428CA0 reaches428D5B and setsB0 bit8. Thus even an empty Node section refreshes
the initial directional payload before the own Light section changes type.
Source spLight constructor now sets the same marker. The later ambient
producer intentionally leaves those initial directional fields untouched.
Scalar CP88 source setup clears the constructor marker through qualified
base Light world, matching its existing explicitly prepared nativeB0=0;
its19 exact cases remain unchanged.

Source tests deserialize the actual LightData wire through production
spLightDataSerializer, call the virtual world method, and pass the same
spDXLight into production light submission and constant resolution. The
view and native ambient/default-vector globals are explicit inputs. The
light list is a borrowed prepared list; scene registration and selection,
first lit shader generation, full SMO frame and GPU execution are separate.
No cached payload or raw constructor override replaces the decoded object.

Final reports under local-data/results/bounded-native-runs:
20260907T141420297421Z-pc-skin-decoded-light.json11/11;
20260907T141420296433Z-pc-light-serializer.json19/19 regression.
Initial141111247475Z mismatch is preserved as failed evidence and excluded
from exact totals. 822 linked assertions plus99 shared draw assertions
give921. Cumulative CP51..91:319 exact/9443 native assertions, plus38
native-only boundary assertions from CP88.

Full build and CTest61/61 passed after the constructor fix in62.15s.
The subsequent first-generation candidate remains explicitly bounded in
[its separate note](native-pc-skin-decoded-light-generated-boundary.md).
