# ParticleCtrl Animation file descriptor raw DWORD getter (PC)

The typed own publisher of `wxParticleCtrl` binds the descriptor labelled `Animation file` to getter `0x00555050`. Its complete seven-byte body reads the receiver's DWORD at `ECX + 0x124` (292) into EAX and returns. The receiver is dereferenced for that four-byte read; the returned DWORD is not itself dereferenced by this leaf. Ownership comes from ParticleCtrl's own registration and publisher chain, rather than adjacency to this shared callback.

The getter-only value facade requires its explicit PC profile and primary `0x00701C8C` in a supplied 296-byte prefix. It returns four unchanged little-endian bytes as an uninterpreted `uint32_t`. All 32-bit patterns are accepted. Neither the label nor the source method name establishes a filename, string, pointer, handle, resource ownership or particle-animation meaning. No floating-point conversion is performed.

The supplied prefix is read-only; other bytes remain opaque. Wrong profiles and primaries leave the output unchanged. Output storage overlapping the prefix is unqualified. The original setter at `0x005880A0` and its string/allocation/lifetime behavior are excluded; no setter or returned-word access is exposed.

The selected read ends at byte 296. Historical factory allocation 412 and proposed diagnostic guard span 328 are separate dimensions and do not establish current native `sizeof`. Current construction, complete clone behavior, physical inheritance, formal native signatures, production callers, PS2, resource loading, particle simulation/rendering and gameplay remain unqualified. Registration base `wxEntity` is metadata, not proof of physical C++ inheritance. This getter does not close the ParticleCtrl class.
