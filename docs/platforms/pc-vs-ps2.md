# PC и PlayStation 2

Статус: чистые PC- и PS2-корпусы проиндексированы отдельно; конкретные layouts
по-прежнему считаются подтверждёнными только после структурной проверки.

## Полный корпус 2026-08-27

Общая schema v3 содержит `pc-pristine`, отдельно изменяемый `pc-working` и
`ps2-pristine`. Все 416 SMO каждого PC-корпуса и 317 уникальных PS2 SMO разобраны
без ошибок. В PS2-архивах сохранены также все 1 481 физическое вхождение SMO:
они дедуплицируются для статистики форм, но учитываются при анализе использования.

| Показатель | PC pristine | PS2 pristine |
|---|---:|---:|
| Уникальные SMO | 416 | 317 |
| Объекты | 177 369 | 160 387 |
| Прямые serializer-поля | 1 112 916 | 1 037 225 |
| Class ID | 36 | 32 |

32 класса являются общими. Только на PC встречаются
`spMaterialColorController`, `spFont`, `spTextRenderable` и `spTextNode`; PS2-only
классов не найдено. Это отрицательное наблюдение по всему доступному корпусу, но
не доказательство отсутствия соответствующих функций: PS2 может кодировать их в
полях другого класса.

Два PC-корпуса не взаимозаменяемы: из 4 217 общих путей 1 025 имеют разное
содержимое, включая 29 SMO. Подробная схема и воспроизводимые команды описаны в
[`../research/smo-corpus-database.md`](../research/smo-corpus-database.md).

Первый полностью закрытый общий класс — `spFog`: PC и PS2 executable используют
один 20-байтовый field 0 (`type`, ARGB color, `start`, `end`, `density`) и те же
смещения членов объекта. У всех 314 одноимённых PC/PS2 SMO fog payload совпадает
побайтно. Это подтверждение именно данного класса; оно не переносится
автоматически на platform-specific mesh/texture payload.

Следующий закрытый общий класс — `spOBBBV`. Оба executable используют поля
position=0, full size=1 и quaternion rotation=2; во всех доступных объектах
сериализован только 12-байтовый size. Совпадают мультимножества OBB во всех 87
одноимённых PC/PS2-ресурсах, включая SMO с несколькими box. Подробности:
[`../research/smo-class-sp-obbbv.md`](../research/smo-class-sp-obbbv.md).

Базовый `spNode` также использует один общий PC/PS2 serializer из девяти полей.
По эффективным transform/flags и отношениям совпадают 288/317 одноимённых
PC/PS2-ресурсов; остальные имеют платформенно разное содержимое графа, но тот же
layout. PS2 использует sized/inline child relationships, а две PC-сцены содержат
дополнительную корректную ID-only форму field 5. Подробности:
[`../research/smo-class-sp-node.md`](../research/smo-class-sp-node.md).

## Актуальная граница платформенного разбора

| Область | PC-корпус | Ресурсы `_ps2` | Уверенность |
|---|---|---|---|
| Контейнер | `FFPS`, little-endian каталог | также наблюдается `FFPS` | высокая для изученных файлов |
| Platform mask `0x10` | loader принимает биты common `1` или PC `2` | loader принимает биты common `1` или PS2 `8` | подтверждено disassembly и runtime rejection PS2-only mask на PC |
| Mesh field `1` | cross-platform E0 и Direct3D E1; пять `_ps2.smo` также содержат native PS2 E1 | native PS2 header + DMA/VIF qwords | boundaries и header подтверждены; команды/vertex channels PS2 ещё opaque |
| Vertex layout | 13 D3D layouts; у skinned `*3E/*7E` disk stride меньше runtime на 12 байт | native channels находятся внутри DMA/VIF | PC corpus закрыт кроме optional attributes `0x013E`; PS2 channel decode отложен |
| Texture/material | PC BGRA, graph и наблюдаемые material layouts разобраны | PS2 formats/palette/mip boundaries разобраны | PS2 swizzle/descriptor semantics и часть runtime states открыты |

Сам факт одинаковой внешней сигнатуры не доказывает полностью одинаковый serializer или renderer path. Варианты следует выбирать по структуре и полям файла, а не только по имени `_ps2`.

## Проверенная сводка 2026-08-10

Новый сравнительный корпус включает прямые пары Bird, Butterfly, Fish, Kikko и Gardenia01, а также восемь вариантов Bloom и дополнительные GUI/loading resources.

| Область | PC | PS2 | Статус |
|---|---|---|---|
| Platform mask | `0x02` | `0x08` | подтверждено runtime-проверками; `0x01` — common |
| Mesh field `1` | обычно DX buffers; пять `_ps2.smo` хранят native PS2 | metadata + DMA/VIF stream | тип определяется внутренним layout, не корпусом |
| Mesh field `2` | отсутствует | 24-byte `minXYZ/maxXYZ` | подтверждено на всех 20 860 наблюдениях |
| Skin palette capacity | всегда 16 slots | всегда 64 slots | подтверждено всеми 1 758 skin трёх корпусов |
| Kikko body | две palettes: 16 и 9 уникальных bones | одна palette: 20 bones | bone sets и inverse-bind matrices совпадают |
| High-level resources | ANM/SPT/SPL | ANM/SPT/SPL | ряд прямых пар побайтно идентичен |

Для PS2 `E1` в Gardenia01 на всех 973 mesh выполняется
`payloadSize = 0x28 + qwordCount * 16`; первый DMA tag имеет
`QWC = qwordCount - 1`. Bounding sphere в начале `E1` проверена против
соответствующей PC-геометрии. Field `2` является 24-байтовым ordered
`minXYZ/maxXYZ` во всех 20 860 PS2-наблюдениях. Для части прямых пар bounds
охватывают оптимизированное подмножество PC-геометрии; точный exporter algorithm
не восстановлен, но граница и layout поля закрыты.

Object ID и числовые хвосты mesh names не являются стабильными межплатформенными идентификаторами. Сопоставление должно использовать роль, имя, hierarchy и содержимое; `Kikko_alfea.smo` исключён как отдельный PC-вариант.

Полный `spSkin` scan подтвердил 748 объектов в каждой PC-копии и 262 на PS2.
PC хранит 11 968 palette slots на корпус, PS2 — 16 768; все 40 704 inverse-bind
матрицы affine и invertible. Ненулевое первое слово `esfSkin` встречается только
на PC и для всех 66 полностью декодируемых копий равно максимуму активных blend
weights на вершину; поэтому оно учитывается как blend-influence hint, а не
reserved. Подробности: [`../research/smo-class-sp-skin.md`](../research/smo-class-sp-skin.md).

`spMeshBV` также не имеет платформенного layout-разветвления. В pristine PC
строго декодированы 3 609 объектов (98 927 треугольников), на PS2 — 3 295
(93 211). Оба serializer используют version-2 `UInt16` indices/`Vector3`
positions и optional `wxFaceData`. Из 3 295 same-path/ordinal пар field 0
совпадает побайтно в 3 171, полный face payload — в 3 269; остальные являются
различием ресурсов, а не формата. Подробности:
[`../research/smo-class-sp-mesh-bv.md`](../research/smo-class-sp-mesh-bv.md).

`spPartitionRenderable` также использует общий serializer: обязательный
`UInt32` ARGB `DebugColor`, затем 1..67 повторяемых inline relationships на
физические child `spModel`. Для 2 324 same-path/ordinal пар цвет совпадает
полностью, число и упорядоченные имена моделей — в 2 323. Единственное отличие:
PS2 `Alfea03` добавляет `light_ray-000` и `detach ray-000`; это отличие ресурса,
а не layout. Подробности:
[`../research/smo-class-sp-partition-renderable.md`](../research/smo-class-sp-partition-renderable.md).

`spPartitionNode` имеет тот же восьмиполевый serializer на PC и PS2: ARGB
`DebugColor`, обязательные ссылки на `spPartitionSystem`/`spZone`, списки
collision/portal/static объектов и nullable inline `spPartitionRenderable`.
Field 2 `Child` реализован в обоих executable в одном месте writer order. Он
отсутствует у всех 16 204 объектов точного класса, но используется 18 048 раз
в унаследованных секциях `spOctreeNode` как `UInt32 slot + inline relation`.
Из 5 254 общих PC/PS2-пар полностью
совпадают 5 251; три различия относятся к составу ресурсов, а не layout.
Подробности: [`../research/smo-class-sp-partition-node.md`](../research/smo-class-sp-partition-node.md).

`spOctreeNode` также имеет один общий формат: базовая partition-секция с ровно
восемью children и собственные `Vector3 Pivot/Mins/Maxs`. PC offsets
`+0x84/+0xB0/+0xBC`, PS2 offsets `+0x70/+0x9C/+0xA8`, но serialized fields
0/1/2 одинаковы. Все 731 общих пар семантически совпадают; PS2 добавляет 63
узла в `Gardenia03.smo`. Подробности:
[`../research/smo-class-sp-octree-node.md`](../research/smo-class-sp-octree-node.md).

`spPartitionSystem` имеет один общий трёхсекционный формат. Runtime offset root
различается (`+0x1D4` PC, `+0x1E0` PS2), но serialized field 0 одинаков и
обязателен. В 18 общих ресурсах каждой платформы это inline `spBSPNode`; в
остальных — reference на `spOctreeNode`. Все 29 общих систем имеют одинаковый
нормализованный граф; PS2 добавляет систему `Gardenia03.smo`. Подробности:
[`../research/smo-class-sp-partition-system.md`](../research/smo-class-sp-partition-system.md).

`spZone` также имеет общий двухсекционный формат `spNode + spZone`. PC хранит
runtime vector local roots по `+0xB8/+0xBC`, PS2 — array/count по `+0xC0/+0xC4`,
но serialized field 0 одинаков: owned inline `spPartitionNode` или
`spOctreeNode`. Совпадают 122/123 графа; в `Gardenia03` PC zone rootless и
принадлежит `spNode`, тогда как PS2 zone принадлежит `spPartitionSystem` и
содержит один octree root. Подробности:
[`../research/smo-class-sp-zone.md`](../research/smo-class-sp-zone.md).

`spZonePortal` имеет общий односекционный формат с обязательными
`DestinationZone`, `Polygon` и `Open`. В данном случае совпадают даже runtime
offsets PC и PS2: destination `+0x14`, count `+0x18`, vertices `+0x1C`, open
byte `+0x20`. В каждом корпусе 206 объектов, 93 inline и 113 reference
destination; все polygons четырёхвершинные и открытые. Все 206 PC/PS2-пар
совпадают семантически, а различия байтов inline relationship относятся к
вложенной zone и object IDs. Подробности:
[`../research/smo-class-sp-zone-portal.md`](../research/smo-class-sp-zone-portal.md).

`spZonePortalNode` имеет общий двухсекционный формат `spNode + repeated
ZonePortal`. Runtime vector PC расположен по `+0xB8/+0xBC`, а array/count PS2 —
по `+0xC0/+0xC4`; serializer contract и порядок `BackToFront`, затем
`FrontToBack` одинаковы. Все 103 PC/PS2-пары совпадают семантически, 88 — также
побайтно; в остальных 15 отличаются только object IDs двух portal-ссылок.
Подробности:
[`../research/smo-class-sp-zone-portal-node.md`](../research/smo-class-sp-zone-portal-node.md).

`spBSPNode` имеет общий двухсекционный формат `spPartitionNode + Plane/Polygon`.
Plane normal/constant находятся по runtime offsets `+0x84/+0x90` на PC и
`+0x70/+0x7C` на PS2; optional Polygon — по `+0xA4/+0xA8` и `+0x90/+0x94`
соответственно. Во всех 18 общих ресурсах совпадают полная топология и значения
108 плоскостей; 91/108 сериализованных поддеревьев совпадают побайтно, остальные
различаются служебными object IDs. Подробности:
[`../research/smo-class-sp-bsp-node.md`](../research/smo-class-sp-bsp-node.md).

`spOcclusionVolume` также использует общий двухсекционный формат
`spNode + IndexBuffer/VertexBuffer`. Runtime pointers различаются: Vertex/Index
на PC находятся по `+0xC8/+0xD0`, на PS2 — по `+0xD0/+0xD8`, но serialized
fields полностью одинаковы. Во всех восьми общих ресурсах совпадают 20/20
объектов, включая node transforms, заголовки буферов, UInt16 indices и все
Vector3 positions. Отдельной platform-ветки decoder не требуется. Подробности:
[`../research/smo-class-sp-occlusion-volume.md`](../research/smo-class-sp-occlusion-volume.md).

`spMeshNavigationSet` использует общий трёхсекционный формат
`spNode + spNavigationSet + Mesh` при разных runtime offsets. На PC NodeCount и
матрицы лежат по `+0xB4/+0xB8/+0xBC`, Enabled/Mesh — по `+0xE1/+0xE4`; на PS2
это `+0xC0/+0xC4/+0xC8` и `+0xE9/+0xF0`. Все 117 PC/PS2-пар имеют одинаковые
node/portal routing matrices, ordered links, placement, flags и target classes;
108 сериализованных объектов совпадают полностью. Девять отличий ограничены
relationship identity и вложенными descendants. Два PC test-world объекта не
имеют PS2-пары. Подробности:
[`../research/smo-class-sp-mesh-navigation-set.md`](../research/smo-class-sp-mesh-navigation-set.md).

## Полная сводка `spMeshData` 2026-08-27

Schema v2 и чистые PC/PS2-корпуса разрешили группы раннего scan. Строго
декодированы все 66 191 уникальный mesh: 22 649 в каждом PC-корпусе и 20 893
на PS2. Прежние 494 «PS2 preamble» и 137 «boundary variant» на PC являются
соответственно native-only и cross+native мешами в пяти `Menus/*_ps2.smo`, а
не повреждениями. Всего таких PS2-native представлений на один PC-корпус 629.

`primitiveType = 2` подтверждён как triangle list и поддерживается; в чистом
PC-корпусе это 14 Direct3D mesh. Все PS2 field `1` удовлетворяют формуле
`40 + dmaQwordCount × 16`, а 20 860 field `2` являются конечными ordered
`minXYZ/maxXYZ`. Stale offsets исходного смешанного набора не воспроизводятся
на pristine-корпусе. Подробный отчёт:
[`../research/smo-class-sp-mesh-data.md`](../research/smo-class-sp-mesh-data.md).

## Выполненный baseline чистого сравнения

1. SHA-256 и metadata нетронутой PC-установки сохраняются как `pc-pristine`.
2. Рабочая/модифицированная установка хранится отдельно как `pc-working`.
3. Все 78 PS2 PCK индексируются без публикации или обязательного извлечения ресурсов.
4. Одна ревизия parser сохраняет объекты и поля всех трёх наборов в schema v3.
5. Class presence, FFPS target mask и manifest PC-различий доступны готовыми запросами.
6. Для каждого нового варианта или runtime-эксперимента выбираются минимальные
   структуры и создаются синтетические fixtures без игровых данных.

История завершённого разбора 36 классов, схема платформенных меток и правила
учёта повторов внутри PCK находятся в
[`../research/smo-class-analysis-plan.md`](../research/smo-class-analysis-plan.md).
Следующий этап описан в
[`../research/smo-runtime-validation-plan.md`](../research/smo-runtime-validation-plan.md).

## Критерий подтверждения платформенного варианта

Ветка parser для PC/PS2 считается обоснованной, если одновременно выполнены условия:

- признак находится внутри файла и стабилен на нескольких независимых ресурсах;
- границы всех прочитанных блоков сходятся без signature scan;
- decode не ломает уже подтверждённый корпус другой платформы;
- различие отражено synthetic test;
- при необходимости runtime-поведение подтверждено игрой или executable analysis.

До этого новые случаи должны завершаться понятной диагностикой, а не попыткой автоматически угадать layout.
