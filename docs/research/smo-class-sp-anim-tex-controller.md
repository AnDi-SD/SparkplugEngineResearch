# Полный разбор `spAnimTexController`

`spAnimTexController` (`0x16FB0E47`) имеет один содержательный field 0:

```text
UInt32 frameCount
Single time[frameCount]
relationship<spTexture> texture[frameCount]
```

Времена конечны, строго возрастают; число times и textures обязано совпадать с
`frameCount`. Native serializer проверяет базовый Class ID `spTexture`
(`0x2F281E13`). Все texture relationships разрешаются, а конкретные inline-объекты
texture-data дополнительно проходят соответствующий decoder.

Строго декодированы 28 controllers (9/9/10) и 974 frames. Найдены три storage
variants:

| Frames | Inline textures | Объектов |
|---:|---:|---:|
| 38 | 9 | 16 |
| 38 | 0 | 9 |
| 8 | 2 | 3 |

Всего 150 inline и 824 reference frames. Tracks идут примерно с частотой 30 Hz:
38-frame до `1.266667 s`, 8-frame до `0.266667 s`. Все 9 PC-пар совпадают
побайтно; все общие PC/PS2 tracks имеют одинаковые времена и классы textures.
PS2 добавляет один MusaX controller. Полные bytes платформ различаются из-за
native-представления inline textures.

Воспроизводимый отчёт: [`analyze_smo_anim_tex_controller.py`](../../research/analyze_smo_anim_tex_controller.py).

Структура sequence закрыта. Runtime пока должен различить loop/clamp/restart и
точное правило выбора frame после последнего timestamp; быстрый тест включён в
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
