# Scale in BookStateMachine (PC)

PC registration identifies `wxBookStateMachine` as `0x396C4B37` with primary `0x006FAF20`. The publisher at `0x0052E0F0` binds `Scale` at receiver offset `+0x224` to getter `0x0051D460` and setter `0x0052DE60`.

The getter loads one binary32 field with `FLD`. The captured profile starts with an empty x87 stack and control word `0x037F`. It returns the ten physical bytes of `ST(0)` for finite normal values and either signed zero. The setter copies the raw DWORD at `[ESP+4]` to the field and returns with `RET 4`. It executes the store even when the DWORD already matches.

The captured prefix is 552 bytes and ends immediately after `Scale`. A standalone write accepts any raw DWORD and preserves all other bytes. A composed read, write and read accepts finite normal values and both signed zeros in the selected word and the argument. It returns both x87 results and the complete updated prefix. An admission failure preserves the output.
