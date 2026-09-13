# spBoxBV

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spBoxBV](../../../Sparkplug/Code/Sparkplug/spBoxBV.h).

## Наблюдаемая структура

Каждый объект занимает 23 байта и имеет форму `s0:f1:12|s0:end`:

| Смещение | Размер | Значение |
| ---: | ---: | --- |
| `0x00` | 4 | class ID `0x7B4C0876` |
| `0x04` | 4 | `SBOO` |
| `0x08` | 2 | field 1: header `0xA1`, UInt8 payload size `0x0C` |
| `0x0A` | 4 | `Single size.x` |
| `0x0E` | 4 | `Single size.y` |
| `0x12` | 4 | `Single size.z` |
| `0x16` | 1 | terminator собственной serializer-секции `0x00` |

Значения little-endian. Как и у `spOBBBV`, сериализуется полный размер, а не
half-extents. Loader обеих платформ выполняет:

```text
halfExtents = size * 0.5
boundingSphereRadius = length(halfExtents)
```

Для двух наблюдаемых объектов это даёт:

| Ресурс | Half-extents | Runtime sphere radius |
| --- | --- | ---: |
| `Qc.smo` | `(35.6828384, 69.8450623, 37.6441689)` | `86.9981674` |
| `blooming_flower.smo` | `(101.717133, 272.380615, 109.554276)` | `310.708407` |

## Полный набор полей

PC и PS2 executable независимо подтверждают один и тот же контракт:

| Field | Имя | Payload | Поведение serializer |
| ---: | --- | --- | --- |
| 0 | `esfBoxBVPosition` | `Vector3` | локальная позиция; нулевой вектор опускается |
| 1 | `esfBoxBVSize` | `Vector3` | полный размер; записывается всегда |

Field 0 — реальная возможность формата, хотя ни один доступный SMO её не
использует. Внешний transform принадлежит родительскому `spCollisionInfo`, а
это поле задаёт дополнительное локальное смещение самого box.

Rotation-поля у `spBoxBV` нет. Этим он отличается от `spOBBBV`, у которого field
2 хранит quaternion. Следовательно, найденный класс — действительно более
простой axis-aligned box volume, а не сокращённая форма OBB.

## Подтип и база

Структурный подтип один — `box_bv_size_only`. Ему назначены все шесть объектов.
Два разных вектора являются параметрами, а не отдельными подтипами.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spBoxBV
```

Viewer показывает эти значения в read-only панели «Показывать восстановленные
поля». Следующий необходимый шаг для editable-статуса — изменить size на копии
одного из двух SMO и проверить форму collision volume в native runtime.
