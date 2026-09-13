# spDXMaterialDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXMaterial](../../../Sparkplug/Code/SparkplugDX/spDXMaterial.h), [spDXMaterialDataSerializer](../../../Sparkplug/Code/Sparkplug/spDXMaterialDataSerializer.h).

Статус: RTTI/lifetime, direct base, storage-free derived ABI и наследуемый material
contract подтверждены в PC и PS2. Это не отдельный material codec.

Class ID — `0x60EE3A89`; прямой base в обеих регистрациях —
`spMaterialSerializer` (`0x2A14745F`). Ни одна vtable не содержит target-hook,
аналогичный `spMaterialDataSerializer`; write/index/read и layer helpers наследуются от
общего material serializer.

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
