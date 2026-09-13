# PC material: exact ABI, scalar codec and pass ownership

## Corrected PC layout

Actual factory41A390 allocates **0xBC**, not the previous guessed0xC4.
Common Material prefix ends at0x78, not PS2-aligned0x80. Engine RTTI is still
Material→BaseObject; physical name+10 is initializedNULL and destructor423AA0
calls NamedObject413090. Secondary material interface is **6DE9D0 at+14**;
previous6DEA20 was wrong. Accessor ECX is complete+14, not complete object.

| PC complete offset | Meaning |
| --- | --- |
| 10 | physical name |
| 18..40 | eleven u32 render states |
| 44 | ctor untouched, included in base-copy twelve-word block |
| 48 / 4C..68 | pass count / eight owning pointers |
| 6C / 6D | render override / raw vertex-alpha byte |
| 70 / 74 | ctor-zero opaque runtime / owning color controller |
| 78 / 88 / 98 / A8 / B8 | diffuse / ambient / specular / emissive / power |

## Original code / portable comparison

- field0:11u32; field1:**rawbyte**, including2, not normalized bool;
- field2:ambient,diffuse,specular,emissive ARGB then powerfloat (20bytes).
  Read424700 multiplies by float1/255. Writer multiplies by255 with x87
  precision then truncates through60DB90 and stores bytes. Safe source only
  writes finite normalized color components; out-of-range CRT behavior is open;
- field3 appends a new owning pass before native blend-word read. Safe host
  validates field and capacity before allocation; no native rollback claimed;
- field6NULL is ignored by reader, **does not clear existing controller**;
- writer order:optional1,always0,passes3,2,always6. Field6 uses UInt32 length
  Begin/End even forNULL, `E60400000000000000`, then section terminator.
