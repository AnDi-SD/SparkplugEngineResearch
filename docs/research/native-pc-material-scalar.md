# PC material: exact ABI, scalar codec and pass ownership

2026-09-06 checkpoint12. Continuing PC SMO/SAN goal, not complete material/render support.
Original PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Corrected PC layout

Actual factory41A390 allocates **0xBC**, not the previous guessed0xC4.
Common Material prefix ends at0x78, not PS2-aligned0x80. Engine RTTI is still
Material→BaseObject; physical name+10 is initializedNULL and destructor423AA0
calls NamedObject413090. Secondary material interface is **6DE9D0 at+14**;
previous6DEA20 was wrong. Accessor ECX is complete+14, not complete object.

| PC complete offset | Meaning | PS2 offset (unchanged evidence) |
|---|---|---|
| 10 | physical name | not re-investigated here |
| 18..40 | eleven u32 render states | 20..48 |
| 44 | ctor untouched, included in base-copy twelve-word block | 4C |
| 48 / 4C..68 | pass count / eight owning pointers | 50 / 54..70 |
| 6C / 6D | render override / raw vertex-alpha byte | 74 / 75 |
| 70 / 74 | ctor-zero opaque runtime / owning color controller | 78 / 7C |
| 78 / 88 / 98 / A8 / B8 | diffuse / ambient / specular / emissive / power | 80 / 90 / A0 / B0 / C0 |

`probe_pc_material_factories.py`: actual material, common/data/DX serializers
(each3C), pass38, texture-layer14 and StdLayer14 factories/dtors all executed.
StdLayer owns its separately allocated material texture**68**, not old guessed6C.
The first strengthened assertion caught that earlier wrong extent; no cap was
involved and the actual allocation104bytes was recorded. Seven factories
now have26 regression assertions. Dumps do not claim that every byte is
initialized or meaningful.
Actual constructor435510 converts existing PC black73FE98/white73FE9C
ARGB bytes through float constant6DCA9C; defaults agree with prior portable values.

## Original code / portable comparison

`probe_pc_material_runtime.py`:27 checks across interface-clone and pass-owners.
Secondary getters/setters preserve raw float bits at the correct complete
offsets. Actual clone41ACF0→copy5A7DB0 leaves constructor defaults, including
NULL name, unlike Fog's named-copy. Actual423960 retains/releases passes,
permits sparse slots and shifts all following slots onNULL removal; count
decrements **once**, not until all trailingNULLs vanish. No out-of-bounds or
sole-owner same-pointer replacement executed. Source guards indices and uses
shared owners; general base-copy graph/policy and controller423650 remain open.

`probe_pc_material_scalar.py` and `compare_pc_material_scalar.py`:five cases,
20 native checks and five exact input/return/cursor/113raw-state-byte/pass-count/
output rows. Cases: empty, changed values, repeated fields/unknown skip,
NULL controller and one empty pass. Reader: secondary42F670→4774D0;
writer: secondary4B0CF0→476B50. Common/DX secondary table6DE514 uses
476B50/4766D0/4774D0, Data table6EFD68 uses4B0CF0/4766D0/42F670.

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

Source uses the existing common DataBlock and reference core, not a separate
format. At checkpoint12 only MaterialData scalars/empty passes were admitted;
[checkpoint13](native-pc-material-standard-graph.md) adds actual DX identity
and standard layers. Remaining unrestored layer/controller branches fail explicitly. Unknown fields
skip and are not yet lossless. Generic base Material copy remains an incomplete
host facade; changing pass pointers to shared owners is not proof of its clone policy.

## Cap and validation boundary

Whole `logo-field` scout over original `Menus/logo_screen.smo` MaterialData
body277..403 (header269, FAT ID4, origin181+offset88) reached the **32KiB
allocation-request guard** with incomplete layer/RTTI startup. No exact failing
request/PC was captured, so the protected cold-RTTI dependency is only a lead,
not an established cause. This native mode is **disabled, never retried or
resumed**, limits unchanged. No complete real material claim follows from the
five scalar fixtures. Asset SHA256:
`DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7`.

31/31 CTests pass32.25sec; new Material suite136checks, Scene suite401
(+20 namedFog). Fog physical-name probe5checks and named-FAT graph9checks
also pass, with exact graph comparison. Earlier failed expectation that common
loading sets Fog name was corrected from actual4671F0 RTTI guard; source core
was already right. No PS2 research credit, game/GPU/API forwarding or asset writes.

Remaining: full layer families/texture/controller links, material base-copy
policy, allocation/error/alias/reentrant paths, whole real material/SMO load/save,
PC materialization/render consumer and lossless unknown preservation.

Final ABI rebuild also corrects MaterialTexture base to68 and keeps unknown68
in the derived target-only prefix.31/31 CTests8.89sec after that correction;
material profile14/14. Immutable checkpoint12 imports six assessments,
including four first separate PC grades with earlier evidence explicitly included.
PC all7.99181446%,engine17.80547112%,old direct37=59.05405405%;PS2 unchanged.
