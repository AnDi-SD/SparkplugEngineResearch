# `spSphereBV` (`0x390946D2`)

Статус: наблюдаемый layout и оба serializer-поля полностью восстановлены для PC
и PS2. Viewer имеет строгий read-only decoder; запись не включена до проверки
изменённой сферы в native runtime.

## Распространённость

| Корпус | Объекты | Уникальные SMO | Физические вхождения | Формы | Payload |
|---|---:|---:|---:|---:|---:|
| `pc-working` | 1 | 1 | 1 | 1 | 1 |
| `pc-pristine` | 1 | 1 | 1 | 1 | 1 |
| `ps2-pristine` | 1 | 1 | 3 | 1 | 1 |

Единственный ресурс — `SFX/vase.smo`. Три PS2 physical occurrence являются
повторными PCK-вхождениями одного и того же файла, а не дополнительными
вариантами. Объект безымянный, имеет object ID 4, непосредственно вложен в
`spCollisionInfo` и не имеет дочерних объектов.

Во всех трёх корпусах поле радиуса побайтно равно `01 1A B8 42`, то есть
`92.0507889f`.

## Наблюдаемая структура

Объект занимает 14 байт и имеет форму `s0:f1:4|s0:end`:

| Смещение | Размер | Значение |
|---:|---:|---|
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

| Field | Имя | Payload | В корпусе | Поведение serializer |
|---:|---|---|---:|---|
| 0 | `esfSphereBVPosition` | `Vector3` | 0/3 | локальная позиция; нулевой вектор опускается |
| 1 | `esfSphereBVRadius` | `Single` | 3/3 | радиус; записывается всегда |

Внешний transform хранится родительским `spCollisionInfo`. Field 0 позволяет
дополнительно сместить центр сферы относительно него, но доступный `vase.smo`
использует нулевой default.

## Свидетельства executable

Pristine PC `WinxClub.exe`:

- строки `spSphereBV`, `spSphereBVSerializer`, `esfSphereBVPosition` и
  `esfSphereBVRadius` имеют registration/code xref;
- load `0x0043A2E0..0x0043A4C1` переключает field ID 0/1; field 0 читает
  `Vector3`, field 1 — `Single` и копирует его в `+0x34` и `+0x24`;
- serialize `0x0043A4D0..0x0043A741` сравнивает position с нулём и опускает её,
  но передаёт radius в serializer без default-сравнения.

PS2 `SLES_532.19`:

- содержит те же четыре строки;
- load `0x0018A2D0..0x0018A454` использует те же field ID, типы и дублирование
  radius в логически те же члены объекта;
- serialize `0x0018A470..0x0018A668` повторяет PC-условия для position/radius.

## Подтип, Viewer и база

Структурный подтип один — `sphere_bv_radius_only`; ему назначены все три
уникальных корпусных объекта. Единственный радиус является параметром, а не
отдельным подтипом.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spSphereBV
```

проверяет форму, родителя, положительный конечный радиус, executable-строки и
точное совпадение 1/1 PC и 1/1 PC/PS2-пары. Она идемпотентно записывает:

- `sphere_bv.position` и `sphere_bv.radius`;
- вариант `sphere_bv_radius_only` и три назначения;
- декодированное значение радиуса;
- четыре evidence-записи: PC, PS2, корпус и cross-corpus.

Read-only панель Viewer показывает field 1 как `Sphere radius = 92.0507889` и
уже сможет декодировать field 0, если нестандартная position встретится в другом
ресурсе. Для editable-статуса остаётся изменить radius на копии `vase.smo` и
проверить реальную collision sphere в игре.
