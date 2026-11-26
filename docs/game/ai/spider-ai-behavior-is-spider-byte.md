# Is Spider field in SpiderAIBehavior (PC)

The PC own publisher `0x00564A60` binds `Is Spider` to one byte at receiver offset `+0x1B0`. Registration and RTTI identify `wxSpiderAIBehavior` with primary `0x006FF840`.

Getter `0x005646A0` loads the field into AL and returns. Setter `0x005646B0` loads one byte from `[ESP+4]`, stores it in the field and returns with `RET 4`. The argument occupies four stack bytes. Both callbacks preserve the upper 24 bits of entry EAX. The low byte stays distinct for all 256 patterns, including `0x02`, `0x80` and `0xFF`.

The analytical profile takes a 433-byte captured prefix with this primary. The getter returns the raw byte. The setter stages the prefix, writes the low byte of a supplied raw word and supports aliasing the input and output. A composed read/write/read provides both byte values and the complete after-prefix. Refused profiles and primaries preserve output. Upper EAX is an observation of the original instructions. The value API exposes the selected byte.

The historical factory allocated 444 bytes. The 433-byte prefix ends immediately after the selected field. Constructor defaults, Boolean interpretation, decision behavior, native return signatures, production callers, object lifetime, PS2 and gameplay remain open.
