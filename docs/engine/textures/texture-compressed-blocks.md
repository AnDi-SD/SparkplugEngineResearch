# PC DXT block decode и original missing-mip путь

## Выполненные оригинальные функции

| Формат | Decode block | Encode block, пока не перенесён |
| --- | --- | --- |
| DXT1 | `64C493` | `64C798` |
| DXT3 | `64C5D5` | `64C8BC` |
| DXT5 | `64C65A` | `64C9EB` |

DXT3 и DXT5 действительно вызывают **тот же `64C493`**, затем заменяют альфу.
При firstColor <= secondColor и colorIndex 3 RGB остаётся чёрным и для этих
форматов. DXT3 раскрывает nibbles через `3D888889`; DXT5 использует две
альфа-ветви и original float constants 1/255, 1/7, 1/5. Имена RGBA относятся
к float block layout; исходный raw upload имеет свой порядок байтов.
