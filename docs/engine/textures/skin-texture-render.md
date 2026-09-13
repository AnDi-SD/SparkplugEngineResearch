# PC decoded texture → material → whole SAN/scene/mesh/Fog/Skin draw

После whole Skin491170 с owning material/Fog/bone и real bbush SAN/scene world,
actual mesh429BC0, отдельный whole TextureData42C640→42C3B0→4ABBA0 читает
нативную mip в actual DXTexture4AB520. Original41E870 передаёт один owning
reference первому material texture holder. Затем весь46A240 через pass4BBBA0
и RTTI resolver4BB650 передаёт тот же COM texture handle в SetTexture(stage0).
Identity E480 обновляется после device; отрицательный HRESULT игнорируется.
