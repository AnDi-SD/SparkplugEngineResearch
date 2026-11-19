# Inflicted Damage descriptor in QuietusCarnivorous

The PC publisher for `wxQuietusCarnivorous` binds `Inflicted Damage` to a raw
32-bit setter/getter pair at receiver offset `0x164`. The setter copies all
four argument bytes and returns with `RET 4`; the getter loads the word into
EAX. Neither callback interprets the word as a floating point value.

The analysis facade accepts a supplied 360-byte prefix carrying the captured
primary vtable identifier. Its explicit raw-DWORD profile reads, stages a write,
or composes read/write/read. Default refusal preserves all output. A staged
write supports input/output aliasing and changes only bytes `0x164..0x167`;
all other captured bytes remain opaque, including other scalar and flag fields.
Raw zero, sign bits, infinities and NaN bit patterns are copied without numeric
normalization or signedness interpretation.

The historical factory allocation was 368 bytes. This does not establish the
current native object size or constructor. Registration, captured primary,
RTTI and the registered publisher establish the selected descriptor binding;
the registration base does not declare physical C++ inheritance. Other
properties, production callers, lifecycle, PS2 behavior, gameplay and the
whole class remain unqualified.
