# SAN (`spAnimation` в FFPS)

SAN использует FFPS-контейнер и на PC хранит `spAnimation` (`0x56EE563A`).
Наблюдаемый основной payload состоит из duration и именованных PRS tracks:

| Field | Содержимое |
| ---: | --- |
| `0` | duration, `float32` |
| `2` | position curve |
| `3` | rotation curve |
| `4` | scale curve |
| `1` | имя track и завершение накопленных PRS curves |

Curve начинается со служебного `UInt32=1` и `keyCount`, затем идут
`float32[keyCount]` времён и `Vector3[keyCount]` либо quaternion XYZW. Имена
track сопоставляются с именами `spNode` регистрозависимо. Missing target не
делает ресурс невалидным; duplicate node names получают all-target binding.

Это основной PC-путь PRS. Существуют дополнительные поля и ветви, не сведённые к этой таблице; полное PS2-воспроизведение не описано.

[Контейнер FFPS](smo.md) · [Анимационные классы](../engine/animation/README.md).
