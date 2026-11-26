# ID, Energy and Ct2 fields in MinotaurAIBehavior (PC)

PC registration and RTTI identify `wxMinotaurAIBehavior` with class ID `0x4E4C79B0` and primary `0x006FFC48`. Its publisher at `0x00567AF0` binds three properties in the order below.

| Property | Receiver offset | Getter | Setter | Analytical scope |
| --- | --- | --- | --- | --- |
| `ID #` | `+0x1B0` | `0x0054ED20` | `0x00563230` | Raw DWORD |
| `% Energy to call next minotaur` | `+0x1B4` | `0x004FD790` | `0x00563240` | Raw DWORD store and bounded x87 load |
| `Is in Ct2 Cells:` | `+0x1B9` | `0x00567700` | `0x00567710` | Raw byte |

The ID getter loads all 32 bits into EAX. The ID setter copies the DWORD from `[ESP+4]`. The Ct2 getter loads AL, and its setter copies the low byte of the stack argument. Both Ct2 callbacks preserve the upper 24 bits of entry EAX. Each setter returns with `RET 4`.

The ID and Ct2 profile accepts a 442-byte captured prefix carrying the Minotaur primary. Its readers and writers carry the raw widths above. Each composed read/write/read returns the raw before and after values and the full after-prefix. Both writers support aliasing the input and output. A refused profile or primary leaves the output intact.

The ID write covers `+0x1B0` through `+0x1B3`. The Ct2 write covers `+0x1B9` only. Both preserve the four Energy bytes at `+0x1B4`, the adjacent byte at `+0x1B8` and every other unselected byte.

## Energy

The Energy getter loads the binary32 word with `FLD`. The qualified call starts with an empty x87 stack and control word `0x037F`. Its result is the full ten physical bytes of `ST(0)`. The Energy setter copies the DWORD at `[ESP+4]` to `+0x1B4` through `+0x1B7` and returns with `RET 4`. An equal argument still executes the native store.

The separate Energy profile takes the same 442-byte captured prefix. Its standalone setter accepts any raw DWORD and preserves the remaining bytes, including ID, Ct2 and `+0x1B8`. Its getter and composed read/write/read accept finite normal binary32 values and both signed zeros. The composed operation requires both the original value and the new word to meet that condition. It returns both ten-byte values and the full after-prefix. A refusal preserves output.

Exceptional x87 behavior, Energy interpretation, Ct2 truth semantics, publisher and writer execution, constructor, AI state and lifetime, production callers, PS2 and gameplay remain open.
