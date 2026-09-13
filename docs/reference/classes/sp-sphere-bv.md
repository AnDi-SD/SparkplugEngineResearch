# spSphereBV

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spSphereBV](../../../Sparkplug/Code/Sparkplug/spSphereBV.h).

## Наблюдаемая структура

Объект занимает 14 байт и имеет форму `s0:f1:4|s0:end`:

| Смещение | Размер | Значение |
| ---: | ---: | --- |
| `0x00` | 4 | class ID `0x390946D2` |
| `0x04` | 4 | `SBOO` |
| `0x08` | 1 | compact field header `0x61`: field 1, payload 4 байта |
| `0x09` | 4 | `Single radius = 92.0507889` |
| `0x0D` | 1 | terminator собственной serializer-секции `0x00` |

Payload хранит именно **радиус**, не диаметр и не квадрат радиуса. Это
подтверждается именем `esfSphereBVRadius` и loader-кодом обеих платформ: значение
копируется в собственный radius сферы и одновременно в radius базового bounding
volume.

## Полный набор полей

| Field | Имя | Payload | Поведение serializer |
| ---: | --- | --- | --- |
| 0 | `esfSphereBVPosition` | `Vector3` | локальная позиция; нулевой вектор опускается |
| 1 | `esfSphereBVRadius` | `Single` | радиус; записывается всегда |

Внешний transform хранится родительским `spCollisionInfo`. Field 0 позволяет
дополнительно сместить центр сферы относительно него, но доступный `vase.smo`
использует нулевой default.

## Подтип, Viewer и база

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spSphereBV
```

Read-only панель Viewer показывает field 1 как `Sphere radius = 92.0507889` и
уже сможет декодировать field 0, если нестандартная position встретится в другом
ресурсе. Для editable-статуса остаётся изменить radius на копии `vase.smo` и
проверить реальную collision sphere в игре.
