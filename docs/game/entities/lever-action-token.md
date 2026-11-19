# Lever Action descriptor raw DWORD getter (PC)

The typed own publisher of `wxLever` binds the descriptor labelled `Action` to getter `0x00555050`. Its complete seven-byte body reads a DWORD at primary-receiver offset `+0x124` (292) into EAX and returns. This shared callback is attributed through Lever's own registration and publisher chain, rather than by code adjacency.

The getter-only value facade requires its explicit PC profile and primary `0x00700910` in a supplied 296-byte prefix. It returns the four unchanged little-endian bytes as an uninterpreted `uint32_t`. Every bit pattern is accepted, including patterns which could represent non-finite floats under a different interpretation. It performs no floating-point conversion and does not interpret the value as a string, pointer, handle or gameplay action. The name `ActionTokenRaw` identifies the descriptor's raw value without assigning token semantics.

All prefix bytes are read-only and remain opaque. Unknown profiles and wrong primaries leave the output unchanged. No setter is exposed: the original setter wrapper calls helper `0x00599470`, whose transitive allocation, lifetime, aliasing and ownership behavior remains unqualified.

The selected read ends at byte 296. Historical factory allocation 296 and a proposed diagnostic guard span of 328 bytes have separate provenance; their numerical relationship does not establish current native `sizeof`. Opaque current construction, incomplete clone coverage, physical inheritance, formal native signatures, production callers, lifetimes, PS2 behavior, lever activation and gameplay remain open. Registration base `wxEntity` is metadata, not proof of physical C++ inheritance. This getter does not close the Lever class.
