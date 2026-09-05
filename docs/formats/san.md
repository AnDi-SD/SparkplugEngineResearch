# SAN (`spAnimation` в FFPS)

SAN использует FFPS-контейнер и на PC хранит `spAnimation` (`0x56EE563A`).
Наблюдаемый основной payload состоит из duration и именованных PRS tracks:

| Field | Содержимое |
|---:|---|
| `0` | duration, `float32` |
| `2` | position curve |
| `3` | rotation curve |
| `4` | scale curve |
| `1` | имя track и завершение накопленных PRS curves |

Curve начинается со служебного `UInt32=1` и `keyCount`, затем идут
`float32[keyCount]` времён и `Vector3[keyCount]` либо quaternion XYZW. Имена
track сопоставляются с именами `spNode` регистрозависимо. Missing target не
делает ресурс невалидным; duplicate node names получают all-target binding.

Decoder: [`SmoAnimationDecoder.cs`](../../tools/SmoViewer/SmoViewer.Core/SmoAnimationDecoder.cs).
Статистика и игровые probes: [`alfea-unknown-resources.md`](../research/alfea-unknown-resources.md)
и [`smo-runtime-results.md`](../research/smo-runtime-results.md).

Нативный reader/writer поддерживает также поля `5..12` и control field `64`.
Их редкие варианты и полный error rollback ещё не перенесены, поэтому текущий
decoder является строгим для наблюдаемого основного PC-подмножества, но не
полной заменой `spAnimationSerializer`. Нативная карточка:
[`native-class-sp-animation.md`](../research/native-class-sp-animation.md).
