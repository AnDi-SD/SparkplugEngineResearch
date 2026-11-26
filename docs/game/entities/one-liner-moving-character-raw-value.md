# OneLinerMovingCharacter raw DWORD property (PC)

The PC property publisher for `wxOneLinerMovingCharacter` binds `eOneLinerID: ` to getter `0x00555050`. The seven-byte routine loads a DWORD from receiver offset `+0x124` (292) into EAX and returns. The class's typed publisher identifies which descriptor uses the routine. Other classes share the same routine.

`ReadOneLinerIdRawForAnalysis` accepts an explicit PC profile and a 296-byte prefix whose first DWORD is `0x006FE124`. It copies the four little-endian property bytes into the caller's `uint32_t` and preserves the prefix. The default profile, any unknown profile and a different primary each leave the output unchanged. The output must be outside the supplied prefix.

The same property publisher binds setter `0x00555040`. On PC it reads a four-byte stack argument, stores the DWORD at `+0x124`, and returns with `RET 4`. It performs the store even when the value already equals the field. `WriteOneLinerIdRawForAnalysis` applies the same profile and primary checks as the reader. On success it replaces the four property bytes in the caller-owned prefix. A refused write leaves the entire prefix unchanged.

The publisher supplies descriptor type 6 and a null-terminated table of 394 string pointers. Both operations preserve all 32 bits regardless of the descriptor table. The meaning and valid range of the stored values, dispatch, movement and manager effects remain open.

296 bytes is the prefix needed to reach the property's last byte. A historical factory allocates 300 bytes. Native layout, construction, object lifetime, PS2 behavior and gameplay remain open. The registry names `wxEntity` as the registration base. The analysis facade has no C++ base class.
