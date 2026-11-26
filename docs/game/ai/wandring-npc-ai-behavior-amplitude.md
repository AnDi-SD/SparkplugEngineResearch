# Amplitude field in WandringNPCAIBehavior (PC)

PC registration and RTTI identify `wxWandringNPCAIBehavior` with class ID `0x5FC363D2` and primary `0x00700528`. The own publisher at `0x005713D0` binds `Amplitude` to receiver offset `+0x1CC`.

Getter `0x00570CD0` loads the binary32 field onto the x87 stack with `FLD` and returns. The qualified call starts with an empty x87 stack and control word `0x037F`. Its result is the full ten physical bytes of `ST(0)`. Setter `0x00562310` reads the four-byte stack argument, stores its raw bits at the field, and returns with `RET 4`. An equal input value still executes the native store.

The analytical profile takes a 464-byte captured prefix with this primary. Both the starting value and the write argument admit finite normal binary32 values and either signed zero. The setter copies the four bytes exactly. The getter widens the admitted bits to the 80-bit representation and preserves the zero sign. A composed read/write/read returns both ten-byte results and the full after-prefix. A refused profile, primary or value leaves output intact.

The historical factory allocated 576 bytes. The chosen prefix ends after `Amplitude`. The string property `Nav Points Root Name`, publisher execution, exceptional binary32 values, NPC movement effects, PS2, and object lifetime are outside this profile.
