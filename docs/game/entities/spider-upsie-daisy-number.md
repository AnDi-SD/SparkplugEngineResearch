# Spider Num descriptor in SpiderUpsieDaisyAIBehavior

The PC own publisher for `wxSpiderUpsieDaisyAIBehavior` binds `Spider Num`
to a raw 32-bit word at receiver offset `0x134`. Its setter copies all four
argument bytes and returns with `RET 4`; its getter returns the same word
through EAX. The descriptor establishes neither signedness nor a gameplay
interpretation of the raw word.

The analytical facade accepts a supplied 312-byte prefix with captured primary
`0x00700098`. Its explicit raw profile reads, stages a write, or composes
read/write/read. Default refusal preserves output. Staging supports input/output
aliasing and changes only bytes `0x134..0x137`; all other bytes remain opaque.
Every 32-bit pattern is copied without floating point or Boolean normalization.

The historical factory allocation was 332 bytes. The selected 312-byte minimum
prefix and 344-byte guarded harness are separate observations and establish
neither current native size nor object layout. Registration, RTTI, captured
primary, typed clone-to-factory call and the own publisher establish this
selected binding; registration base IDs do not establish physical C++
inheritance. The current factory entry is protected and opaque. Constructor,
native signatures, AI behavior, runtime reachability, lifetime, production
callers, PS2, gameplay and whole-class completeness remain unqualified. The
publisher's Walk Speed and Turn Speed descriptors are outside this raw profile.
