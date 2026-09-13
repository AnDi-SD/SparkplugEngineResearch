# PC unchanged SMO light through complete lit Skin draw

Source tests deserialize the actual LightData wire through production
spLightDataSerializer, call the virtual world method, and pass the same
spDXLight into production light submission and constant resolution. The
view and native ambient/default-vector globals are explicit inputs. The
light list is a borrowed prepared list; scene registration and selection,
first lit shader generation, full SMO frame and GPU execution are separate.
No cached payload or raw constructor override replaces the decoded object.

Full build and CTest61/61 passed after the constructor fix in62.15s.
The subsequent first-generation candidate remains explicitly bounded in
[its separate note](skin-decoded-light-generated-boundary.md).
