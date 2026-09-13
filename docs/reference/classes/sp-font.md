# spFont

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spFont](../../../Sparkplug/Code/Sparkplug/spFont.h), [spTexture](../../../Sparkplug/Code/Sparkplug/spTexture.h), [spTextureData](../../../Sparkplug/Code/Sparkplug/spTextureData.h).

Единственный field 0 имеет layout:

```text
relationship<spTexture> atlas   // runtime; serialized wire resource may be TextureData
UInt32 height
UInt32 baseline
for character 0x20..0xFF:          // ровно 224 записи
    UInt8   width
    Vector2 uv0
    Vector2 uv1
```
