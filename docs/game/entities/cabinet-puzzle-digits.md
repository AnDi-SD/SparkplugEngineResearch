# wxCabinetPuzzle: captured PC combination words

`wxCabinetPuzzle` is a captured-value analysis facade for four own-published callback pairs. In publisher order they are `Combination digit #4`, `#3`, `#2`, and `#1`, stored as four raw bytes at offsets `0x170`, `0x15C`, `0x148`, and `0x134`. The minimum supplied prefix is 372 bytes with the selected primary identity. The labels do not establish signedness or a decimal digit range.

Each getter returns the exact DWORD bits. Each setter stores one supplied four-byte pattern and preserves every other byte. The composed operation performs GET, SET, GET for each pair in publisher order, retaining all four complete after-prefixes and eight getter results. Input and output may alias; unqualified profile, primary or property leaves output unchanged. Raw patterns representing negative integers or floating-point encodings carry no extra interpretation here.

Ownership follows this class's own typed registration and property publisher. Callback addresses may also serve other classes. A historical factory allocation of 468 bytes is distinct from the 372-byte touched extent. Current construction, full layout, physical inheritance, other fields, native signatures, puzzle mechanics, production callers, PS2, gameplay and whole-class behavior remain open.
