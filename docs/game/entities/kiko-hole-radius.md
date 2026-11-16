# wxKikoHole: captured PC Radius

`winx::reconstruction::wxKikoHole` is a captured-value analysis facade for the own-published `Radius` callback pair. It admits a supplied 324-byte minimum prefix with the selected primary identity. The setter preserves one raw four-byte argument at offset `0x140`. The getter loads that binary32 field with `FLD` and returns its exact ten-byte extended payload, including the sign of zero.

The named analysis profile admits finite binary32 normal values and signed zero for the captured field and setter argument. Subnormals, infinities and NaNs remain outside this profile. Prefix input and output may alias; refusal preserves output. Every other byte is opaque and is preserved by a successful setter.

Exact binary32 widening reuses `spFloat80TowardZero.h` and its `FromFloat` operation. Native FPU stack state, control/status flags and general floating-point arithmetic are not emulated. The getter payload is distinct from a DWORD register return.

The association follows this class's own typed registration and publisher descriptor. Registration inheritance does not establish physical C++ inheritance. A historical allocation of 332 bytes is separate from the minimum captured extent; current construction remains opaque. Units, production use, other properties, native signatures, lifetime, physical layout, PS2, gameplay and whole-class behavior remain open.
