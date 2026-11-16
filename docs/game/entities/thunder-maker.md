# wxThunderMaker: captured PC delay property bits

`wxThunderMaker` supplies a captured-value analysis facade for the own-published `Max Delay (ms)` and `Min Delay (ms)` callback pairs. Each setter loads one four-byte argument, stores the full word at `0x12C` or `0x128`, and returns with RET4. Each paired getter loads the full word into EAX and returns with RET0. No arithmetic, clamp, sorting, sign conversion or delay ordering appears in these leaf bodies.

The qualified analysis profile uses a supplied 304-byte minimum captured prefix with the selected primary identity. All 32-bit field and argument patterns are preserved. The facade stages four-byte writes, permits prefix input/output aliasing, and leaves output unchanged on refusal. The composition reads both fields, sets Max, reads both again, sets Min, and reads both again; each intermediate prefix preserves the other field and every opaque byte.

Replay word tokens are eight hexadecimal digits rendering full 32-bit machine words numerically; captured prefix tokens retain byte order. This analysis representation does not establish a formal native C++ parameter or return type. The publisher labels include `(ms)`, but numeric units, signedness, valid ranges, time scheduling and weather production remain unqualified.

The callbacks are shared with other typed publisher descriptors. Class membership follows this class's own registration, factory/RTTI identity and publisher, rather than callback exclusivity or address adjacency. Registration inheritance does not establish physical inheritance. Historical allocation was 312 bytes; the smaller captured extent is not native sizeof. Current construction, lifetime, other methods, full layout, production callers, PS2, gameplay and whole-class behavior remain open.
