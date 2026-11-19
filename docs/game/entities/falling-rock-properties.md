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

The typed chain combines saved successful factory/RTTI identity with current primary-table RTTI and clone-to-registered-factory references. The six-argument registration names the class's own publisher. Its actual descriptors supply each callback and label; callback adjacency is not used for attribution. The current factory entry is opaque. Constructors, full clone paths, lifetime, field signedness, native C++ signatures, gameplay units, production callers and PS2 behavior remain unknown. The four raw callbacks retain the profile described above. This bounded profile does not close the class or establish runtime/gameplay readiness.

## Six binary32 scalar properties

The separate scalar profile retains the same 396-byte captured prefix and primary identity. It admits finite binary32 normal values and signed zero at the six scalar fields and in setter arguments. The scalar setters copy their raw four-byte arguments. Each getter loads binary32 with `FLD` and supplies a ten-byte little-endian extended value, preserving the sign of zero.

| Publisher label | Captured offset | Setter | Getter |
| --- | --- | --- | --- |
| Gravity acc (cm/s) | `+0x124` | `0x00555040` | `0x00556E30` |
| Rotation speed (rad/s) | `+0x134` | `0x0056A5F0` | `0x00510100` |
| Maximum Heigth delta (cm) | `+0x178` | `0x00590960` | `0x00556E10` |
| Sqr proximty trigger | `+0x174` | `0x00590940` | `0x005101C0` |
| Elasticity coef [0.0,1.0] | `+0x144` | `0x0056B080` | `0x00556DF0` |
| Radius (cm) | `+0x148` | `0x00540CA0` | `0x00556DE0` |

The scalar API stages each result and preserves output on refusal. Prefix input and output may alias. Every byte outside the selected DWORD is preserved. Binary32 widening reuses the existing `FromFloat` provider in `spFloat80TowardZero.h`. The RAW profile independently admits its four arbitrary DWORD fields and opaque prefix bytes.

The scalar composition retains one supplied prefix through GET, SET, GET, SET of the same argument, GET for each property in publisher order. Its values include eighteen getter payloads and twelve complete prefixes. A selected native store may preserve its previous bytes. The returned payload and saved prefix values describe the captured operation, while native C++ return types, FPU stack/control/status behavior, construction, lifetime and production callers require their own contracts. Publisher labels preserve the captured spelling and do not enforce numeric ranges or gameplay units.
