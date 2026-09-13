# spAnimTexController

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spAnimTexController](../../../Sparkplug/Code/Sparkplug/spAnimTexController.h), [spTexture](../../../Sparkplug/Code/Sparkplug/spTexture.h).

`spAnimTexController` (`0x16FB0E47`) имеет один содержательный field 0:

```text
UInt32 frameCount
Single time[frameCount]
relationship<spTexture> texture[frameCount]
```

Строго декодированы 28 controllers (9/9/10) и 974 frames. Найдены три storage
variants:

| Frames | Inline textures |
| ---: | ---: |
| 38 | 9 |
| 38 | 0 |
| 8 | 2 |

Всего 150 inline и 824 reference frames. Tracks идут примерно с частотой 30 Hz:
38-frame до `1.266667 s`, 8-frame до `0.266667 s`. Все 9 PC-пар совпадают
побайтно; все общие PC/PS2 tracks имеют одинаковые времена и классы textures.
PS2 добавляет один MusaX controller. Полные bytes платформ различаются из-за
native-представления inline textures.

Структура sequence закрыта. Runtime пока должен различить loop/clamp/restart и
точное правило выбора frame после последнего timestamp; быстрый тест включён в
`smo-runtime-validation-plan.md`.
