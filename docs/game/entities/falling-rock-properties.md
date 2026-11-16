# Falling rock raw properties

The captured PC profile for `wxFallingRock` exposes four raw DWORD property pairs in the object's own publisher order. Each getter loads four bytes into EAX. Each setter copies the four argument bytes from the stack into the corresponding field; it does not convert the value.

| Publisher label | Captured offset | Setter | Getter |
| --- | --- | --- | --- |
| Warning Delta (ms) | `+0x188` | `0x0056C530` | `0x00556E20` |
| Falling Duration (ms) | `+0x184` | `0x00541CA0` | `0x00539560` |
| Falling Interval (ms) | `+0x180` | `0x00541C90` | `0x00541C80` |
| Inflicted Damage | `+0x170` | `0x005427C0` | `0x00556E00` |

The profile requires the captured primary table `0x006FE288` and retains a minimum396-byte prefix through the final selected field. The historical factory allocated476bytes. These sizes describe distinct observations; neither declares a current native `sizeof` or physical inheritance.

The value facade defaults to an unqualified profile. Successful reads return the full raw32-bit value; successful writes retain every byte outside the chosen DWORD. Refusal leaves output untouched. The composed operation retains state through GET, SET, GET for each of the four properties, recording each getter result and each complete prefix immediately after its setter. Host CPU/stack resets between native callbacks would be a fixture control rather than a production caller claim.

The typed chain combines saved successful factory/RTTI identity with current primary-table RTTI and clone-to-registered-factory references. The six-argument registration names the class's own publisher. Its actual descriptors supply each callback and label; callback adjacency is not used for attribution. The current factory entry is opaque. Constructors, full clone paths, lifetime, field signedness, native C++ signatures, gameplay units, production callers and PS2 behavior remain unknown. Six FLD float getters and all other methods are excluded. This bounded profile does not close the class or establish runtime/gameplay readiness.
