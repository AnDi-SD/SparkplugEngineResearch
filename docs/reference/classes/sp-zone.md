# spZone

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spZone](../../../Sparkplug/Code/Sparkplug/spZone.h).

## Итоговая структура

`spZone` наследует `spNode` и имеет две serializer-секции:

```text
spNode section
  field 0  Position                  # обязательно
  field 1  Rotation?                # необязательное поле
  field 4  IsStatic = true?         # необязательное поле
  field 8  IsAnimated = false       # обязательно
  terminator

spZone section
  field 0  LocalPartitionRoot[]     # 0..4 owned inline roots
  terminator
```

Scale, IsBone, Child, BillboardAxis и Collision поддерживаются унаследованным
serializer, но ни одна зона их не записывает. Для них действуют defaults
`(1,1,1)`, `false`, пустой список, `0` и пустой список. Position задаёт пространственную координату sector; в известных ресурсах она сериализована явно.

## LocalPartitionRoot

Строки и reader обоих executable называют declared type `spPartitionNode`, но
отношение полиморфно: конкретной целью может быть и наследник `spOctreeNode`.
Все наблюдаемые roots:

- ненулевые;
- записаны как `objectId`, `inlineSerializedSize`, затем полный SBOO;
- физически принадлежат своей `spZone`;
- разрешаются только в `spPartitionNode` либо `spOctreeNode`.

| Zones | `spPartitionNode` roots | `spOctreeNode` roots | Rootless zones |
| ---: | ---: | ---: | ---: |
| 123 | 126 | 11 | 1 |
| 369 | 378 | 34 | 2 |

Распределение числа roots на одну зону одинаково у двух PC-копий: один объект
имеет 0, 114 — один, три объекта — по два, три — по три, два — по четыре. На PS2 rootless-объекта
нет: 115 зон имеют один root, остальные распределены как на PC. Размер одного
inline relationship меняется от 20 949/21 045 байт до нескольких мегабайт;
это размер полного локального partition-поддерева, а не отдельный формат поля.

## Родители и смысл directory nesting

Физический родитель зоны зависит от способа первого inline-включения:

- основная зона уровня принадлежит `spPartitionSystem`;
- остальные зоны впервые включаются соответствующим `spZonePortal`, а system
  содержит sized references на них;
- PC `Gardenia03.smo` — единственное исключение: rootless `zone` принадлежит
  обычному `spNode` и в файле нет `spPartitionSystem`;
- PS2-версия `Gardenia03` содержит обычный `spPartitionSystem`, а зона владеет
  одним `spOctreeNode`.

Следовательно, directory parent показывает владельца inline-сериализации, но
не отдельную разновидность `spZone` и не обязательно игровую transform-иерархию.

## PC и PS2

После нормализации object IDs и platform-specific байтов вложенных деревьев совпадают 122 из 123 PC/PS2-графов. Единственное содержательное отличие — описанный выше `Gardenia03`; это реальное различие ресурса, а не диалект serializer.

Обе платформы используют один порядок полей и один class variant. Различаются
только размеры runtime-класса и представление вложенного partition-контента.
