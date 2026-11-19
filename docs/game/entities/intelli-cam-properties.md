# IntelliCam captured scalar properties (PC)

`wxIntelliCam` has a bounded captured-value facade for ten descriptors from its own typed publisher. The separate provided prefix contains 1000 bytes with primary `0x006F522C`. All ten selected fields and setter arguments must be finite normal binary32 values or signed zero; opaque remaining bytes retain arbitrary values.

| Own publisher label | Offset | Setter | Getter |
| --- | --- | --- | --- |
| Camera Mouse Rotation Speed | `+0x3E4` | `0x004DAE60` | `0x004DAE70` |
| Camera Joystick Rotation Speed | `+0x3E0` | `0x00506A60` | `0x004DAE50` |
| Distance Acceleration | `+0x18C` | `0x005914E0` | `0x0058EA30` |
| Distance Stiffness | `+0x1A8` | `0x004DAE20` | `0x004DAE10` |
| Home Elevation | `+0x170` | `0x005427C0` | `0x005427B0` |
| Elevation Acceleration | `+0x188` | `0x0056C530` | `0x004DAE40` |
| Elevation Stiffness | `+0x1A4` | `0x004FC8A0` | `0x00545120` |
| Rotation Acceleration | `+0x184` | `0x00541CA0` | `0x004DAE30` |
| Rotation Stiffness | `+0x1A0` | `0x00545110` | `0x00545100` |
| Y Offset | `+0x14C` | `0x00540CE0` | `0x0053AF80` |

Setters copy four argument bytes without conversion. Getters expose the exact physical ten-byte extended value of the original `FLD`, including signed zero, through the shared `FromFloat` widening provider. Writes stage the complete prefix and permit aliasing. Wrong profiles, primaries, property selectors or out-of-profile words leave outputs unchanged.

The historical factory allocation was 1032 bytes; the selected access end and supplied prefix are 1000 bytes. These observations do not establish current native `sizeof`, constructor behavior or physical inheritance. Registration base IDs describe registration only. The current factory entry is opaque, and the captured clone prefix establishes its direct factory call without closing the whole clone.

Distance Maximum/Minimum and Elevation Maximum/Minimum descriptors have other callback shapes and are outside this scalar profile. This facade calls no constructor, factory, publisher or complete camera update. Native C++ signatures, lifetime, production callers, camera movement, rendering, PS2 behavior, gameplay and whole-class completeness remain open. Display/UI camera settings are described separately in [display and camera behavior](../flow/display-camera.md).
