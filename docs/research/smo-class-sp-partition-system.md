# `spPartitionSystem` (`0x912CC341`)

Статус: завершён полный структурный/read-only разбор наблюдаемого PC/PS2
layout. Все 88 уникальных объектов заново прочитаны из directory/PCK-источников
строгим decoder; 5 587 содержательных direct fields аннотированы в schema v2.
Изменение и пересборка отношений пока не разрешены.

## Итоговая структура

`spPartitionSystem` наследует `spRenderNode`, а тот — `spNode`, поэтому объект
имеет три serializer-секции:

```text
spNode section
  field 4  IsStatic = true
  field 8  IsAnimated = true
  field 5  Child[]
  field 7  CollisionInfo[]
  terminator

spRenderNode section
  field 0  Renderable[]       # в корпусе пусто
  terminator

spPartitionSystem section
  field 0  PartitionRoot      # обязательное отношение
  terminator
```

Отсутствующие node-поля используют уже подтверждённые defaults: position
`(0,0,0)`, identity rotation, scale `(1,1,1)`, `IsBone=false` и billboard axis
`0`. Renderable-список поддерживается унаследованным serializer, но во всех 88
системах пуст.

## Наблюдаемые отношения

| Семантика | PC working | PC pristine | PS2 | Всего | Encoding / target |
|---|---:|---:|---:|---:|---|
| owned zone | 29 | 29 | 30 | 88 | inline `spZone`, физический child системы |
| дополнительные zones | 93 | 93 | 93 | 279 | sized reference на `spZone` |
| portal nodes | 103 | 103 | 103 | 309 | inline `spZonePortalNode`, физический child |
| collisions | 1 549 | 1 549 | 1 549 | 4 647 | sized reference на `spCollisionInfo` |
| BSP roots | 18 | 18 | 18 | 54 | inline `spBSPNode`, физический child |
| octree roots | 11 | 11 | 12 | 34 | sized reference на `spOctreeNode` |

Field 5 node-секции имеет стабильный порядок: одна owned inline-зона, затем
ссылки на остальные зоны, затем inline portal nodes. После них идут все field 7
collision references. В каждом ресурсе присутствует ровно один именованный
`PartitionSystem`.

Главная поправка к ранней гипотезе: собственный `PartitionRoot` не указывает на
`spZone`. Это полиморфный корень partition-дерева. В BSP-уровнях дерево вложено
прямо в поле как `spBSPNode`; в octree-уровнях поле ссылается на существующий
`spOctreeNode`, физически вложенный в зону. Поэтому изменение только одного из
этих графов без согласованной пересборки object IDs, inline sizes и обратных
ссылок небезопасно.

Inline BSP payload имеет размеры 210, 311, 412, 513, 614, 715 или 1 624 байта.
Octree reference всегда занимает 8 байт (`objectId`, нулевой inline size).

## Корпус

| Corpus | Объекты / ресурсы | Serialized size | BSP / octree roots |
|---|---:|---:|---:|
| `pc-working` | 29 / 29 | 1 559 372..7 253 981 | 18 / 11 |
| `pc-pristine` | 29 / 29 | 1 559 372..7 253 981 | 18 / 11 |
| `ps2-pristine` | 30 / 30 | 1 165 033..5 588 029 | 18 / 12 |

PS2 добавляет `data/levels/gardenia/gardenia03.smo`; тип и порядок полей не
меняются. Все 29 PC working/pristine объектов byte-identical. Все 29 общих
PC/PS2-систем имеют одинаковый нормализованный граф после исключения object IDs,
inline-размеров платформенных поддеревьев и их байтового содержимого.

## Свидетельства executable

Pristine PC `WinxClub.exe`:

- reader/index/writer находятся в `0x0044AEF0..0x0044B359`;
- serializer сначала вызывает `spRenderNodeSerializer`;
- строки называют поле `epssfPartitionRoot` и вызов `GetPartitionRoot()`;
- reader запрещает `NULL` и сохраняет указатель по runtime offset `+0x1D4`.

PS2 `SLES_532.19` независимо подтверждает контракт:

- reader/index/writer находятся в `0x001A2720..0x001A2AB4`;
- вызван тот же базовый `spRenderNodeSerializer` и записан тот же field 0;
- обязательный runtime-указатель расположен по `+0x1E0`.

Различие runtime offsets не влияет на serialized layout.

## Viewer и база

`SmoPartitionSystemDecoder` возвращает унаследованный `SmoNodeData`, список
renderables и разрешённый `PartitionRoot`. Он проверяет три секции, writer order,
типы/encoding целей, физическое владение inline-объектами и обязательность root.
Inspector показывает `IsStatic`, `IsAnimated`, все zone/portal/collision
отношения и распознанный BSP/octree root.

Analyzer назначил всем 88 объектам один общий variant, добавил 11 определений
полей (включая поддерживаемые, но не встреченные inherited fields) и четыре
evidence-записи: PC executable, PS2 executable, полный корпус и cross-corpus.
Воспроизведение:

```powershell
python research/analyze_smo_partition_system.py local-data/results/smo-corpus-v2.sqlite
dotnet run --project tools/SmoViewer/SmoViewer.Inspect -- `
  research-db analyze-class local-data/results/smo-corpus-v2.sqlite `
  spPartitionSystem --json
```

Связанные [`spZone`](smo-class-sp-zone.md),
[`spZonePortal`](smo-class-sp-zone-portal.md) и portal nodes уже полностью
разобраны; новых class-by-class шагов для этого graph нет.
