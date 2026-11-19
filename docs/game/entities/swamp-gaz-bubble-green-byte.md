# SwampGazBubble raw Green byte callback value

The own registered publisher labels the byte at primary-receiver offset `0x144`
`Use Green bubbles`. Its setter loads one byte at argument-slot `+4` into AL,
stores AL at the receiver offset and returns with stack adjustment4. The getter
loads the byte into AL and returns without stack adjustment. Both leave upper24
EAX bits unchanged; the instructions perform no Boolean normalization.

The analytic facade uses the existing supplied336-byte captured prefix and
captured primary identity. Its separate opt-in byte profile reads a raw byte,
writes the low byte of an explicit raw argument-slot word, and composes
GET/SET/GET on one staged value. All other336-prefix bytes remain unchanged.
Failure preserves outputs; writing through the input/output alias is supported.
The original rawDWORD Damage profile remains separate and unchanged in scope.

The facade is a raw byte operation, not a declaration of the original C++
signature, Boolean type or complete native return ABI. Historical352-byte
allocation differs from this supplied-prefix bound. Opaque current construction,
physical inheritance, sizeof, lifetime, other properties, game effects and PS2
behavior remain unknown.
