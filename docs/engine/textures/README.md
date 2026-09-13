# Текстуры

Источники и платформенные представления текстуры определяют чтение, mip-уровни и загрузку. [Формат встроенной PC-текстуры](../../formats/smo-textures.md) описан отдельно от ограничения writer конкретного приложения.

<!-- catalog:start -->

## Статьи

- [compressed mip generation и exact block encoding](texture-compressed-mips.md).
- [PC decoded texture + whole first shader generation](skin-texture-generated-render.md).
- [PC decoded texture → material → whole SAN/scene/mesh/Fog/Skin draw](skin-texture-render.md).
- [PC DXT block decode и original missing-mip путь](texture-compressed-blocks.md).
- [PC material → texture: common reference graph (checkpoint 19)](material-texture-links.md).
- [PC native texture data: shared source wrapper and reconstructed reader](texture-native-source.md).
- [PC palette, texture ownership and runtime failure contracts](palette-lifetime.md).
- [PC texture/sampler state mapping](texture-state-map.md).
- [PC texture: runtime flat codec, native mip copy и настоящий registry key](texture-runtime-mips.md).
- [PC texture: source/local codec, DX header и границы upload](texture-codec-boundaries.md).
- [PC TextureData native writer and shared section core](texture-native-writer.md).
- [PC: недостающие mip-уровни raw TextureData](texture-missing-mips.md).
- [PC: общие raw pixels → DXTexture](texture-cross-upload.md).
- [внешний источник TextureData](texture-external-source.md).

<!-- catalog:end -->
