# wxSideQuestGivenObject: captured PC Scale

`winx::reconstruction::wxSideQuestGivenObject` is a captured-value analysis facade for the own-published `Scale: ` property. It admits a supplied 308-byte minimum prefix with the selected primary identity. The raw four-byte store is at offset `0x130`; its paired getter loads binary32 with `FLD` and produces a ten-byte extended value. The analysis result preserves that payload, including the sign of zero.

The named profile admits finite binary32 normal values and signed zero for the captured Scale and setter argument. Subnormal values, infinities and NaNs are outside this profile. The setter stages exactly four bytes and preserves the rest of the supplied prefix; prefix input and output may alias. Refusal preserves output. Other bytes are opaque and add no extra scalar admission requirements.

Exact binary32 widening reuses `spFloat80TowardZero.h` and its `FromFloat` operation. No arithmetic or general floating-point exception model is introduced. The getter payload is distinct from a DWORD register return; native FPU stack state, control/status words and ABI are not emulated by this facade.

The same callback addresses and field offset also serve a separate SwampPlatformRing property. The Scale association follows this class's own typed registration and publisher descriptor. Registration inheritance does not establish physical C++ inheritance. A historical allocation of 312 bytes is separate from the minimum captured extent; current construction is opaque. Native signatures, other properties, lifetime, physical layout, production callers, PS2, gameplay and whole-class behavior remain open.
