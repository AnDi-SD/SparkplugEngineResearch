# `spZone` (`0x61254AB3`)

Статус: завершён полный структурный/read-only разбор общего PC/PS2 layout.
Строгий decoder повторно прочитал из исходных directory/PCK-ресурсов все 369
уникальных объектов, разрешил 412 отношений и аннотировал 1 231 содержательное
direct field в schema v2. Изменение и пересборка графа пока не разрешены.

## Итоговая структура

`spZone` наследует `spNode` и имеет две serializer-секции:

```text
spNode section
  field 0  Position                  # обязательно
  field 1  Rotation?                # 2 объекта на corpus
  field 4  IsStatic = true?         # 25 объектов на corpus
  field 8  IsAnimated = false       # обязательно
  terminator

spZone section
  field 0  LocalPartitionRoot[]     # 0..4 owned inline roots
  terminator
```

Scale, IsBone, Child, BillboardAxis и Collision поддерживаются унаследованным
serializer, но ни одна зона их не записывает. Для них действуют defaults
`(1,1,1)`, `false`, пустой список, `0` и пустой список. Position — настоящая
пространственная координата sector, а не техническое поле: все 123 объекта
каждого corpus сериализуют её явно.

## LocalPartitionRoot

Строки и reader обоих executable называют declared type `spPartitionNode`, но
отношение полиморфно: конкретной целью может быть и наследник `spOctreeNode`.
Все наблюдаемые roots:

- ненулевые;
- записаны как `objectId`, `inlineSerializedSize`, затем полный SBOO;
- физически принадлежат своей `spZone`;
- разрешаются только в `spPartitionNode` либо `spOctreeNode`.

| Corpus | Zones | `spPartitionNode` roots | `spOctreeNode` roots | Rootless zones |
|---|---:|---:|---:|---:|
| `pc-working` | 123 | 126 | 11 | 1 |
| `pc-pristine` | 123 | 126 | 11 | 1 |
| `ps2-pristine` | 123 | 126 | 12 | 0 |
| всего | 369 | 378 | 34 | 2 |

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

Все 123 объекта `pc-working` byte-identical соответствующим объектам
`pc-pristine`. После нормализации object IDs и platform-specific байтов
вложенных деревьев совпадают 122 из 123 PC/PS2-графов. Единственное
содержательное отличие — описанный выше `Gardenia03`; это реальное различие
ресурса, а не диалект serializer.

Обе платформы используют один порядок полей и один class variant. Различаются
только размеры runtime-класса и представление вложенного partition-контента.

## Свидетельства executable

Pristine PC `WinxClub.exe`:

- index/reader/writer находятся в `0x0044D720..0x0044DC82`;
- базовая секция передаётся `spNodeSerializer`;
- vector локальных roots хранится по `+0xB8/+0xBC`;
- reader создаёт declared class `0x67672341` (`spPartitionNode`);
- writer повторяет `ezsfZoneLocalPartitionRoot`, `WriteBegin`,
  `SerializeRelationship`, `WriteEnd` для каждого элемента.

PS2 `SLES_532.19` независимо подтверждает тот же контракт:

- reader/index/writer находятся в `0x001A3B30..0x001A3F70`;
- factory hash также равен `0x67672341`;
- runtime array/count находятся по `+0xC0/+0xC4`;
- строки `No empty spZone->spPartitionNode` запрещают null-элемент, но пустой
  список roots допустим.

## Viewer и база

`SmoZoneDecoder` проверяет обе секции, node mask/order, обязательное
`Animated=false`, inline encoding, concrete target class, serialized size и
физическое владение root. Inspector показывает Position, optional Rotation и
Static, Animated и каждое разрешённое `Local partition root`.

Analyzer назначил всем 369 объектам один общий variant, добавил 10 определений
полей, 369 assignments и четыре evidence-записи. Воспроизведение:

```powershell
python research/analyze_smo_zone.py local-data/results/smo-corpus-v2.sqlite
dotnet run --project tools/SmoViewer/SmoViewer.Inspect -- `
  research-db analyze-class local-data/results/smo-corpus-v2.sqlite `
  spZone --json
```

Связанные `spZonePortal`, `spZonePortalNode`, BSP и navigation-классы уже
полностью разобраны. Дальнейшие вопросы относятся к runtime-поведению переходов,
а не к границам serializer этого класса.
