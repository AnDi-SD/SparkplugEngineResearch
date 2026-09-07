# PC `spBSPNode`: lifetime, queries и выбор Zone

Checkpoint 19, 6 сентября 2026. Это runtime-дополнение к
[wire-карточке](smo-class-sp-bsp-node.md), не заявление о восстановлении всего
класса или исправлении Viewer. Pristine PC SHA и оригинальные instructions
проверяются скриптами ниже; PS2 повторно не исследовался.

## Идентичность и исходник

Original RTTI `spBSPNode` **7362AB22 → spPartitionNode67672341 → spBaseObject**;
это не наследник spNode. Registration CALL6D4230, record7613F8/base75E1B8,
factory480B90 через protected slot13B23E4. Exact native size **AC**,
primary table6EBA30 имеет33 slots. Factory/lifetime реально исполнены;
не приписываем отдельному адресу предполагаемый constructor symbol.

Добавлены частичные `Sparkplug/Code/Sparkplug/spBSPNode.h/.cpp`,
`Tests/spBspTests.cpp`, observed ABI `spBSPNodeLayoutAC` и ray record8.
Путь реализации **inferred**: limited ASCII search не находит BSP .cpp/.h,
также нет `spBSPNodeSerializer.cpp`, хотя serializer class name есть.
`GetPlane`, `GetPolygonVertex`, `GetPolygonVertexCount` — original names из
diagnostics; остальные descriptive API намеренно `ForAnalysis`.

| Runtime поле | Доказанное назначение |
|---|---|
| 00..83 | inherited PartitionNode, включая owned child array58/count5C |
| 84..90 | split plane normalXYZ/constant; constructor **не инициализирует** |
| 94/98,9C/A0 | две внутренние ray `{child,parameter}` записи, initially untouched |
| A4/A8 | directly owned optional polygon XYZ array / vertex count, initially0 |

Factory создаёт именно два owned null child slots. Native setter44CDA0 просто
копирует16 bytes плоскости, без normalization; reader вызывает его44D038.
Setter480210 independently frees/copies polygon, count×12, **не вычисляет и
не меняет plane**; reader вызывает44CFD5. Проверены counts3/5/0 и reinit.
Ненулевой указатель после zero-size allocation — свойство bounded allocator
fixture, не универсальное утверждение о native malloc(0).

Clone480C10: новая factory + RegisterClone412F70 + inherited Base40ECE0.
Plane/polygon/children не копируются; новое собственное состояние blank.
Deleting480450 → destructor480180 → inherited destruction; owned polygon и
child array освобождаются. Source использует explicit unknown-plane flag,
host null/index/count4096 guards и owned vector; это не exact host ABI и не
заявление о таких проверках в оригинале.

## Точные запросы к плоскости

Distance = `(z*nz + y*ny) + nx*x - constant`, native x87.

| Slot / entry | Правило |
|---|---|
| 40 /480710 | Если stopAtZone и Zone60, вернуть this до проверки plane. Иначе distance>0 → child0, всё остальное → child1; затем child.v40 |
| 44 /480780 | distance>+0.001f →mask1, distance<−0.001f →mask2, band/NaN →mask3 |
| 48 /4807D0 | abs(distance)<=radius →mask3; иначе positive→1, прочее→2 |
| 5C /480200 | unchecked two-word table740374:8/1, три бита на child, order0,1 или1,0 |
| 54 /4D6550 | no-op: BSP не выключает clip planes как Octree |

Таким образом **положительная сторона — slot0**, отрицательная и точка ровно
на plane в leaf lookup — slot1. Это доказано исполнением, а не статистикой
координат SMO. PointMask и lookup различаются на epsilon band: Debug traversal
берёт первый установленный mask bit, поэтому его starting side на plane может
отличаться от strict leaf query. Не заменять эти функции одной общей эвристикой.

## Ray candidates и видимые дети

Slot4C/480360 принимает originXYZ/directionXYZ и optional output pointer.
Normal·direction и distanceOrigin сначала сохраняются как **float**.
Первая запись всегда `{distance>0 ? 0 : 1,+0}`. Вторая появляется, только если
направление пересекает plane вперёд: denominator>+1e-5f из child1 или
denominator<−1e-5f из child0. Она `{child^1,-distance/denominator}`.
На plane допустим second parameter−0; parallel/away дают одну запись.
Нет проверки максимальной длины луча. Native возвращает borrowed `this+94`,
перезаписываемый следующим запросом; unused tail untouched. Source возвращает
host-owned копию результата, это явно не native lifetime/API.

Slot50/480C60 проверяет optional split polygon против каждого **enabled**
clip plane; cached activeCount игнорируется. Для каждого plane нужна хотя бы
одна vertex со строго distance>0.001f. Если такая не найдена, видимой считается
только camera-side ветвь; это **не** отбрасывание всего BSP node.
Empty polygon при enabled plane даёт одного ребёнка; zero planes/all disabled
дают двух. В четырёхвершинных unrolled blocks сумма X+Z+Y, scalar tail Z+Y+X;
это различие сохранено в analytical arithmetic.

Camera-side здесь distance>**0.001f**, не strict-zero leaf rule. Packed word:
low4=count, далее3bits/child. Both/positive camera =82, both/other=12;
one/positive=01, one/other=11. Native null children не фильтруются.

## Размещение и whole Scene

RenderNode slot28/480540:

- Zone60 + dynamic, либо static billboard (B0 mask300000): touching sphere
  остаётся в текущем BSP; non-touching идёт в единственный child;
- no Zone либо ordinary static400: sphereMask48, все выбранные children;
- actual reciprocal Node1C4/Partition20 lists подтверждены шестью сценариями.

Collision slot1C/480470 и Occlusion slot34/480630 имеют родственный Zone/static
разбор (collision ownerNode10/B0/sphere5C; occlusion B0/sphere1A4, без billboard
исключения). В C19 это был **static mapping**; subsequent checkpoint20
исполнил Occlusion branch, тогда как Collision остаётся static-only.
Static slot58/480AD0 использует sphere38, но mask48 лишь **без Zone**;
при Zone touching sphere остаётся в текущем BSP. Этот ранее неполно описанный
nonempty путь и Occlusion insertion теперь исполнены в
[checkpoint20](native-pc-spatial-consumers.md); Collision пока static-only.
Debug slot64/480830 рисует polygon edges/closing edge и normal из centroid;
его полный native draw/helper consumer ещё не проверен.

Whole original Scene45EC70 с actual BSP/двумя Partition leaves/двумя Zone и
двумя RenderNodes прошёл3 frames: cameraX+1 выбирает только positive-room node,
cameraX−1 и0 — только negative-room node. Actual Node world выполняет placement,
actual NodeAttach/ZoneAppend создают связи, actual cleanup освобождает graph.
Fixture задаёт decoded graph явно; это **не исполнение SMO reader**.

Visibility46D270 сначала вызывает root.v40(cameraPosition,stopAtZone=1),
затем проходит **local roots выбранной Zone**. Этот путь не требует обычного
recursive BSP child clipping из root. Он не закрывает capped45E870 copy из
Octree/portal-near-plane и не доказывает полную normal recursive traversal.
Helpers не заменялись unconditional success и capped calls не продолжались.

## Проверки и остаток

- `python research/probe_pc_bsp_runtime.py`: **48/48** (16 lifetime +15 leaf/ray
  +12 registration +5 whole Scene); four fresh bounded children;
- `python research/inspect_pc_bsp_runtime.py`: **17/17** pristine/RTTI/table/
  constants/reader/getter/path anchors;
- `python research/compare_pc_bsp_queries.py`: **2046/2046 fields /256cases**
  (1020+1026), включая epsilon/NaN, negative radius, no polygon, plane flags/
  inconsistent activeCount и near-parallel ray. All383 ray parameters bit-exact
  на этой выборке; tolerance2e-6 не является universal x87 equivalence;
- C++ source **17/17**, full187-step build и **CTest21/21**.

После [checkpoint20](native-pc-spatial-consumers.md) открыто: nonempty Collision
registration, остальные64..7C
debug/collision/ray consumers, runtime producers optional polygon, reparent/
setup и lifetime ошибок, protected plane-vector copy/full Visibility ctor.
Исходник остаётся partial: **нет portable Scene registration/render integration**.
Никаких GPU/image/game tests или изменений Viewer/импортера/ресурсов.
