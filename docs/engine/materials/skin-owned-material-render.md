# PC Skin-owned material + SAN/scene/mesh → draw

Два FAT entries: Node ID7/class695C0F65 и Material ID8/class6160348B.
Whole491170 теперь читает Renderable field0 с inline MaterialData body,
затем Model terminator и Skin field0 с bone reference/inverse bind.
Actual47FBA0→4678B0→42F4C0/42F670 создаёт DXMaterial, pass и два StdLayer.
Reader инкрементирует material+8 и записывает Skin20; после завершения
serializer/FAT lifetimes материал остаётся жив с одним owning ref.
Трёхэлементный RTTI input объявлен явно, original membership/factory работают.
