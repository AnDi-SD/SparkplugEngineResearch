# PC decoded texture → material → whole SAN/scene/mesh/Fog/Skin draw (CP81)

2026-09-07; pinned pristine PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

После whole Skin491170 с owning material/Fog/bone и real bbush SAN/scene world,
actual mesh429BC0, отдельный whole TextureData42C640→42C3B0→4ABBA0 читает
нативную mip в actual DXTexture4AB520. Original41E870 передаёт один owning
reference первому material texture holder. Затем весь46A240 через pass4BBBA0
и RTTI resolver4BB650 передаёт тот же COM texture handle в SetTexture(stage0).
Identity E480 обновляется после device; отрицательный HRESULT игнорируется.

**6 exact captures /480 native assertions** (71 linked+9 render каждый):
raw,DXT1,DXT3,DXT5,failed-device,post-false. Read Skin74022 instructions,
texture8703..8706,render15592..15595. Peak arena65120..65128 bytes;
113 engine owner generations освобождены реальными destructors, COM device
ref вернулся к1, texture/surface к0, поверхность разблокирована. Padding4
bytes каждой поверхности остался A5. Поля1C/24 — bytes; соседняя native CC
padding не входит в семантический capture. Native44 остаётся CC, bytecount48=0.

Source SubmitUnlitGeometry теперь разрешает actual spDXTexture после
проверки dynamic type и берёт identity/palette из объекта. Явный callback
поставляет только внешний COM handle; существующие state/binding/cache
реализации выполняют внутренние действия. Source reader получает те же
bytes, подтверждённый pitch=row+4 и сохраняет packed mip bytes. Holder владеет
shared source texture. Alternate RTTI resource families остаются открытыми.

COM fixture compact texture page34070000 содержит только external device,
texture/surface vtables/callbacks и pixel storage. Все engine allocations
остаются в64KiB arena с прежним32KiB request cap;100000 instructions/2s call
и30s child не изменены. Native texture reader и материал setter выполнены
отдельной завершённой фазой: **это не texture reference внутри whole SMO read**.
Синтетический малый SMO graph, prepared scene/RTTI/cache, реальные SAN bytes;
GPU и внутренняя работа COM не исполнялись. Cross-format conversion не заявлена.

```powershell
python research/native_workbench.py run pc-skin-texture-render --deadline-utc 2026-09-07T16:00:00Z
```
