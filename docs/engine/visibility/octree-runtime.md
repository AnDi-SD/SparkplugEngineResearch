# PC `spOctreeNode`: исходные запросы и частичный source

## Runtime identity и исходники

Original `spOctreeNode21A70829→spPartitionNode67672341→spBaseObject415352A1`.
Registration`75DA88`, factory`41A760`/slot`13B19CC→459020`, ctor`4493B0`/
slot`13B2F0C`. **Полная factory/ctor выполнена**, allocation **C8**.
Primary`6E4420`,33 slots, dtor`449420→4264D0`, deleting`449AE0`.
ASCII-поиск `spOctreeNode.cpp/.h` в этом PC executable отрицательный;
пути новых файлов `Code/Sparkplug/spOctreeNode.*` **inferred**, не выданы за PDB.

Добавлены original-named portable **частичные** классы:

- `spPartitionNode`: direct-owned child slots, presence-only Zone query input,
  leaf-query identity, inherited Base-only clone;
- `spOctreeNode`: eight-child constructor, explicit geometry input, leaf/point/
  sphere/plane queries и original traversal order.

Это **не все33 methods**, не полный Scene/serializer/partition registrations.
`unique_ptr`, invalid-index/null-child/finite-bounds guards — host-only.
Native Zone ownership не заменяется bool: bool в source явно представляет
только проверяемое query40 условие «Zone присутствует», без runtime Zone объекта.

## Layout и lifetime

Base84; Pivot84 (12bytes), scratch90 (**четыре пары** childIndex/floatRayT),
MinsB0/MaxsBC. Ctor выделяет отдельный массив **8 nullable pointers/32bytes**
через base58/count5C; **Pivot и scratch не инициализируются**, Mins/Maxs zero.
Source держит geometryKnownfalse, не утверждает, что native Pivot изначально0.
Runtime scratch90 — результат ray query, не восемь child pointers и не wire fields.
Reader`44C8E0` прямо записывает Pivot/Mins/Maxs, midpoint не вычисляет.

Clone`41B100`: freshfactory +register`412F70`+Basecopy`40ECE0` через v0C.
Реальный заполненный graph→clone: восемь новыхnullslots, Zone0, bounds0,
Pivot/scratch остаются allocatorCC. Дети и геометрия не копируются.
Destructor прямо удаляет детей даже при ненулевом intrusive refcount и
освобождает slot array. Zone ref у parent intrusive, остальные Scene refs живут.

## Запросы и точные границы

| Slot /address | Подтверждённая роль |
| --- | --- |
| 40 /449E50 | Leaf by point; stopAtZone &&Zone60 возвращаетthis до чтенияPivot |
| 44 /449430 | Point mask, epsilon0.001f вокруг каждой координатыPivot |
| 48 /449530 | Sphere octant mask через squared-distance/table shortcuts |
| 4C /449690 | Ray start+direction → borrowed scratch90/count≤4 |
| 50 /44AA30 | Plane-set child candidates, packed low4count +3bits per child |
| 54 /44AC60 | Отключает уже полностью пройденные planes для child bounds |
| 58 /449A90 | StaticRenderObject sphere38 → child v58 по mask48 |
| 5C /449520 | Original eight packed traversal-order words740150 |

Leaf octant bitsXYZ0/1/2: **строго point>Pivot**, equality иunordered→low.
Нет native null-child guard. PointMask44 отдельно допускает обе стороны при
`abs(point-pivot)<=0.001f` (арифметика x87 перед сравнением, не float midpoint).

### Важное отличие SphereMask от обычной геометрии

Это может быть уже математически точного eight-orthants overlap:
Pivot0, sphere`(.7,.7,.9,r1)`→**F0**, только children4..7.
Sphere также пересекает child3 (distance к z=0 всего.9), но native shortcut
его не добавляет. Аналоги по осям даютAA/CC. Мы не «улучшали» этот ответ в
восстановленном source. При `(1,1,1,r1)` ответE8, включая касания трёх planes.
Negative radius возводится в квадрат, не нормализуется/не отвергается.
Это контракт движка; **причина прежнего бага LevelCreator этим не установлена**.

### Плоскости и порядок

Для каждого octant bounds берутся из `Mins..Pivot` /`Pivot..Maxs` независимо
по осям. Query50 выбирает extreme vertex вдоль normal и исключает child при
`dot(n,p)-d < -0.001`. `activeCount` не gate; enabled-byte каждого plane важен.
Далее camera strict octant задаёт порядок; camera0:
`0,1,2,4,3,5,6,7`, остальные по original таблице.
Return word low4=count, далее3bits/child отbit4; nullchildren не фильтруются.

Query54 выбирает opposite extreme vertex. Если distance>+0.001, plane
disabled и cached activeCount unsigned--. Tangency/NaN остаются active;
не происходит автоматического пересчёта неконсистентного count.
Native record tests сравнены с source на160+160 independent cases,
включая NaN, epsilon boundaries, negative radii и count mismatch.

Ray4C: input `{originXYZ,directionXYZ}`, first record=startoctant,t0.
Для каждой из3 split planes при `abs(dir)>1e-5f` иt>=0 вычисляется crossing,
точка классифицируется чуть дальше по лучу (`t+abs(1/dir)*0.001f`),
duplicate octants не добавляются, остальные sorted byT; original scratch
перезаписывается следующим вызовом. Исполнены4 cases: раздельные3 crossings,
одновременное диагональное пересечение, away ray и zero direction.
Portable ray source пока не добавлен, граничные near-parallel/NaN случаи open.

Actual RenderNode registration`449C10`:

- при Zone60 и dynamic node (либо static billboard сmask300000) sphere,
  касающаяся хотя бы одного split plane, остаётся в **текущем node**;
  иначе единственный child по center;
- без Zone либо static nonbillboard — mask48, append во все выбранные children;
- reciprocal RenderNode1C4/PartitionNode20 lists совпадают с этим выбором.

## Visibility integration: что не закрыто

Свежий whole Scene с actual Octree/eight leaves проходит registration, но
normal clipped traversal останавливается на **100k** внутри protected
plane-vector copy`45E870`, slot`13B1DE8→13B5E20→A0D3E0`.
Независимый minimal-copy scout также capped100k; fragment остаётся
защищённым, не объявлен декомпилированной готовой копией. Caps не повышались,
вызовы не продолжались и helper не заменён unconditional success.

Отдельно whole Scene исполнен на **настоящей Debug21 unclipped ветке**:
actual Node world размещает8 объектов по8 leaves, original46B870 обходит
их и вызывает все8 support draw в order`0,1,2,4,3,5,6,7`.
Это не доказательство нормального clipped прохода. Debug выбирает starting
octant как первый установленный bit **epsilon PointMask44**, тогда как normal
query50 использует strict camera octant; различие околоPivot сохранено.
