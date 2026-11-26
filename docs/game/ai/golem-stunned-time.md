# Stunned Time field in GolemAIBehavior (PC)

The PC own publisher `0x00566680` binds `Stunned Time (ms)` to one DWORD at receiver offset `+0x1D8`. The typed class registration identifies `wxGolemAIBehavior` with primary `0x006FFA90`.

Getter `0x004FD7B0` reads the four bytes into EAX. Setter `0x00565BF0` copies all four argument bytes to the field and returns with `RET 4`. Both operations preserve the raw bit pattern, including words with the high bit set and equal-value stores.

The analysis profile takes a 476-byte captured prefix and checks the primary and profile before returning a value. A write stages the prefix, changes only the selected four bytes, and supports aliasing the input and output. The composed operation captures the value before the write, the complete after-prefix, and the value read back. Refusal preserves output.

The historical factory allocated 484 bytes. The selected prefix ends immediately after the examined field. Constructor defaults, numeric signedness, valid values, processing of the stun timer, other properties, native layout, PS2 and gameplay remain open.
