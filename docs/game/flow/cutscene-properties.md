# Cutscene captured F32 property (PC)

`wxCutscene` exposes one bounded captured-value operation at `+0x1BC` (444): setter `0x00564080` copies four unchanged argument bytes, and getter `0x005126F0` executes `FLD` and exposes its exact physical ten-byte extended value through the shared `FromFloat` provider, including signed zero.

The own publisher names this descriptor with bytes `50 72 6F 78 69 6D 69 74 79 20 72 61 64 69 75 73 20 28 63 6D B2 29`, followed by NUL. A Latin-1 rendering is `Proximity radius (cm²)`; the original runtime text encoding, numeric units and any squared-distance interpretation remain unqualified. The source selector `ProximityRadius` identifies those exact bytes and callbacks without adding a unit conversion.

The facade accepts a supplied 448-byte prefix with primary `0x006F8134`, finite normal binary32 or signed zero in the selected field, and the same argument domain. Other bytes are opaque and preserved. Writes stage the complete prefix and permit input/output aliasing. Wrong profiles, primaries, selectors or out-of-domain values leave outputs unchanged.

A historical factory allocation of 472 bytes is separate from the selected access end and provided prefix of 448 bytes. Registration base IDs do not establish physical C++ inheritance. The current factory entry is opaque; the captured clone prefix establishes its direct factory call without closing the complete clone. No native size, signatures, constructor, lifetime, production callers, scene triggering, PS2 behavior, rendering, gameplay or whole-class completeness is established. Animation Name and Cinematic Name descriptors have other callback shapes and remain outside this scalar profile.

This value operation is distinct from [CinematicGameFlowState messages and input switching](cinematic-game-flow-state.md).
