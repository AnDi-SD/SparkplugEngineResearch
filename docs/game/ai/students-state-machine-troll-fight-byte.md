# Troll Fight byte in StudentsStateMachine (PC)

PC registration identifies `wxStudentsStateMachine` as `0x78F35E7F` with primary `0x006FB280`. Its publisher at `0x00531A40` binds `Troll Fight` at receiver offset `+0x284` to getter `0x005318B0` and setter `0x005318A0`.

The getter loads the raw byte into `AL`. The setter copies the low byte of the DWORD at `[ESP+4]` to `+0x284` and consumes the four-byte argument. All 256 byte values round-trip unchanged. The store preserves every other byte in the captured prefix.

The captured profile takes 645 bytes from a receiver with the recorded primary. It returns the old and new raw bytes and the complete updated prefix for a composed read, write and read.
