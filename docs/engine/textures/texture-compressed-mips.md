# compressed mip generation и exact block encoding

Это default filter4 без dithering, подтверждённый actual codec callback
arguments. Произвольные conversion modes, dithering и live GPU остаются открыты.
Число классов и scores не менялось.

## Original block encoders

| Формат | Original entry | Восстановленная ветвь |
| --- | --- | --- |
| DXT1 | `64C798` → `64BB40` | RGB565 endpoints, 3/4 colors, threshold0,5, transparent block |
| DXT3 | `64C8BC` → `64BB40` | 4-bit explicit alpha и four-color block |
| DXT5 | `64C9EB` → `64BB40`/`64B299` | optimized6/8 alpha palette и four-color block |
