# Guard Num descriptor in UpsieDaisyGuardAIBehavior

The PC publisher for `wxUpsieDaisyGuardAIBehavior` binds `Guard Num` to a raw
32-bit word at receiver offset `0x184`. Its setter copies all four argument
bytes and returns with `RET 4`; its getter returns the same word through EAX.
The descriptor binding does not establish signedness or a gameplay meaning
for the raw word.

The analytical facade accepts a supplied 392-byte prefix with the captured
primary identifier. Its explicit raw profile reads, stages a write, or composes
read/write/read. Default refusal preserves output. Staging supports input/output
aliasing and changes only bytes `0x184..0x187`; other bytes remain opaque.
All 32-bit patterns are copied without floating point or Boolean normalization.

The historical factory allocation was 488 bytes. The selected minimum prefix
and the synthetic guarded fixture are separate from that allocation and do
not declare native object layout or current `sizeof`. Registration, RTTI,
captured primary and the own publisher establish this selected binding;
they do not declare physical C++ inheritance. Constructor, other descriptors,
native signatures, AI/runtime behavior, lifetime, production callers, PS2,
gameplay and the whole class remain unqualified.
