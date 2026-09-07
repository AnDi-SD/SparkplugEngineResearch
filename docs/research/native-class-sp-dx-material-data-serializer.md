# `spDXMaterialDataSerializer`: DX material marker

PC checkpoint12: [actual3C factory / secondary6DE514](native-pc-material-scalar.md).
Actual table retains common476B50/4766D0/4774D0; source marker inherits the
same bounded material codec. No additional DX-specific payload invented.

PC checkpoint13: header42F4C0 is shared with MaterialDataSerializer and
unconditionally creates spDXMaterial after eight-byte read. This class is
**not** the distinct runtime spDXMaterialSerializer177E2F26 registered for
797B39EC at6D4C80. See [exact PC graph](native-pc-material-standard-graph.md).

Статус: RTTI/lifetime, direct base, storage-free derived ABI и наследуемый material
contract подтверждены в PC и PS2. Это не отдельный material codec.

Class ID — `0x60EE3A89`; прямой base в обеих регистрациях —
`spMaterialSerializer` (`0x2A14745F`). Ни одна vtable не содержит target-hook,
аналогичный `spMaterialDataSerializer`; write/index/read и layer helpers наследуются от
общего material serializer.

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E588` | `0x004AB050` |
| Initializer | `0x006D2C90` | `0x00483D10` |
| Factory | `0x0042F3E0` (protected entry) | `0x001A5250` |
| Allocation | observed `0x3C` | exact `0x3C` |
| Destructor | `0x0042F3C0` | `0x001A5100` |
| Blank clone | `0x0042F450` | `0x001A5170` |
| Registration getter | `0x0042F3B0` | `0x001A5070` |
| Signature/load helper | `0x0042F4C0` | `0x001A5080` |
| Primary vtable | `0x006DE520` | header `0x00490170` |
| Secondary vtable | `0x006DE514` | header `0x00490194` |

PS2 factory вызывает constructor `spMaterialSerializer` и меняет только два vptr;
дополнительных members нет. Secondary slots указывают на общие thunks
`0x00195E50/0x00195E40/0x00195E30`, а layer helpers — на тот же набор, что у base.
PC дополнительно показывает общий secondary vtable с `spPS2MaterialDataSerializer` и
общий signature/load helper.

Portable-класс поэтому восстанавливает только RTTI и concrete blank clone, а стандартный
write/index plan получает наследованием. Platform payload и target ID намеренно не
придуманы.

Открыты original header/source path, интерфейсные имена/signatures, остальные allocation/error ветви,
семантика platform selection и in-game mutation test.
