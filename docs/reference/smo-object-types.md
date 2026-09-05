# Классы объектов SMO игрового корпуса

Дата актуализации: 2026-08-28. Табличные частоты ниже относятся к 416 PC-файлам
`pc-working`: 177 369 записей object directory и 36 уникальных class ID. Полный
многокорпусный аудит дополнительно включает 416 `pc-pristine` и 317 уникальных
PS2 SMO. Все 1 149 копий разобраны без ошибок, неизвестных class ID нет.

После завершения очереди все 36 встреченных классов имеют строгий
structural/read-only decoder. Статус «полный read-only» означает полный разбор
наблюдаемого layout, но не гарантирует безопасную запись или знание всей
runtime-семантики. Эти ограничения перечислены в
[`../../research/open-questions.md`](../../research/open-questions.md).

## Сцена и размещение

| Hash | Класс | Объектов | Файлов | Назначение | Статус |
|---:|---|---:|---:|---|---|
| `0x763277DB` | `spModel` | 40 555 | 311 | mesh/material/fog и render-order binding | полный read-only decode PC/PS2 |
| `0x695C0F65` | `spNode` | 17 841 | 416 | transform, hierarchy, bones и service nodes | полный read-only decode PC/PS2 |
| `0x603625D0` | `spRenderNode` | 14 064 | 413 | renderable node и связь с visual resources | полный read-only decode PC/PS2 |
| `0x56D67170` | `spStaticRenderObject` | 20 469 | 29 | статическое world-space размещение уровня | полный read-only decode PC/PS2 |

## Геометрия, материалы и текстуры

| Hash | Класс | Объектов | Файлов | Назначение | Статус |
|---:|---|---:|---:|---|---|
| `0x33C34CF0` | `spMeshData` | 22 649 | 396 | vertex/index buffers и mesh bounds | полный структурный read-only decode PC/PS2 |
| `0x6160348B` | `spMaterialData` | 36 334 | 412 | material state, passes и texture/controller links | полный структурный read-only decode PC/PS2 |
| `0x78EA082B` | `spTextureData` | 2 564 | 373 | встроенные raster textures | полный структурный read-only decode PC/PS2 |
| `0x681F2043` | `spSkin` | 748 | 117 | renderable/model state, bone palette и inverse-bind matrices | полный read-only decode PC/PS2 |
| `0x1C0053D6` | `spUVController` | 1 607 | 186 | управление UV transform/animation | полный read-only decode PC/PS2 |
| `0x4C633E85` | `spMaterialColorController` | 2 391 | 31 | пять material evaluator-секций | полный read-only decode PC + PS2 executable verification |
| `0x16FB0E47` | `spAnimTexController` | 9 | 6 | временная последовательность texture frames | полный read-only decode PC/PS2 |

`spMaterialColorController` ранее ошибочно выглядел как многочисленный
безымянный navigation helper: 1 644 его экземпляра находятся в одном
`BMS_04B.smo`. Регистрационная строка EXE однозначно задаёт настоящее имя.
Полный корпус и serializer-код подтвердили один 54-байтовый вариант с evaluator
ambient/diffuse/specular/emissive/alpha; подробности находятся в
[`../research/smo-class-sp-material-color-controller.md`](../research/smo-class-sp-material-color-controller.md).

`spUVController` содержит один вложенный `spTransFunctionEval`: translation XYZ,
scale XYZ, rotation, UV pivot и rotation axis. Одиннадцать размеров являются
разреженными формами с опущенными default-полями, а не подтипами. Полный разбор:
[`../research/smo-class-sp-uv-controller.md`](../research/smo-class-sp-uv-controller.md).

`spTextureData` имеет пять storage-вариантов: legacy cross-platform BGRA,
embedded Direct3D BGRA, PS2 native, PS2 native с cross-копией и embedded
cross-only. Все 7 485 объектов трёх корпусов структурно декодированы вместе с
палитрами и mip-цепочками. Значения `0x32E3`/`0x0EE3` оказались байтами заголовка
и размера поля, а не pixel formats. Полный разбор:
[`../research/smo-class-sp-texture-data.md`](../research/smo-class-sp-texture-data.md).

`spMaterialData` использует строгую последовательность из 11 material render
states, одного–трёх проходов с девятью texture states, цветов, optional static
UV и object relationships. Подтверждены четыре legacy/current ×
single/multi-pass варианта и полностью декодированы 105 588 объектов трёх
корпусов. Полный разбор:
[`../research/smo-class-sp-material-data.md`](../research/smo-class-sp-material-data.md).

`spMeshData` имеет три поля: cross-platform geometry, platform-specific
Direct3D/native PS2 representation и PS2 AABB. Все 66 191 уникальных объекта
трёх корпусов распределены по семи вариантам; раскрыты оба PC buffer layouts,
13 Direct3D vertex formats и native PS2 header/DMA boundary. В пяти `_ps2.smo`
PC-корпуса находятся 629 настоящих PS2-native mesh. Полный разбор:
[`../research/smo-class-sp-mesh-data.md`](../research/smo-class-sp-mesh-data.md).

## Collision и bounding volumes

| Hash | Класс | Объектов | Файлов | Назначение | Статус |
|---:|---|---:|---:|---|---|
| `0x47A97C0E` | `spCollisionInfo` | 3 643 | 194 | collision owner, group, transform и primitive relationship | полный read-only decode PC/PS2 |
| `0x3F453DE7` | `spMeshBV` | 3 609 | 95 | triangle collision mesh + optional wxFaceData | полный read-only decode PC/PS2 |
| `0x4DA04889` | `spOBBBV` | 148 | 97 | oriented box bounding volume | полный read-only decode PC/PS2 |
| `0x7B4C0876` | `spBoxBV` | 2 | 2 | axis-aligned box bounding volume | полный read-only decode PC/PS2 |
| `0x390946D2` | `spSphereBV` | 1 | 1 | spherical bounding volume | полный read-only decode PC/PS2 |

`spCollisionInfo` имеет одну секцию Primitive/optional Group/optional Transform.
Все 10 594 экземпляра трёх корпусов декодированы и распределены по трём формам;
все primitive relationship разрешаются в `spMeshBV`, `spOBBBV`, `spBoxBV` или
`spSphereBV`. Полный разбор:
[`smo-class-sp-collision-info.md`](../research/smo-class-sp-collision-info.md).

`spMeshBV` field 0 хранит общий PC/PS2 version-2 triangle list, field 1 —
необязательный массив `wxFaceData` с surface type, flags и surface ID. Все
10 513 экземпляров трёх корпусов декодированы; полный разбор:
[`smo-class-sp-mesh-bv.md`](../research/smo-class-sp-mesh-bv.md).

Для `spOBBBV` field 1 хранит полный размер `(X,Y,Z)`, а не half-extents.
Необязательные field 0 position и field 2 quaternion rotation подтверждены
кодом обеих платформ, хотя доступные SMO их не сериализуют. Полный разбор:
[`smo-class-sp-obbbv.md`](../research/smo-class-sp-obbbv.md).

`spBoxBV` использует тот же full-size контракт, но имеет только field 0 position
и field 1 size, без rotation. Все шесть экземпляров трёх корпусов имеют
size-only форму; полный разбор:
[`smo-class-sp-box-bv.md`](../research/smo-class-sp-box-bv.md).

`spSphereBV` имеет optional field 0 position и обязательный field 1 radius.
Единственный `SFX/vase.smo` использует radius `92.0507889`, одинаковый на PC и
PS2. Полный разбор:
[`smo-class-sp-sphere-bv.md`](../research/smo-class-sp-sphere-bv.md).

`spNode` имеет общий девятиполевый PC/PS2 serializer: transform, bone/static/
animated flags, child/collision relationships и billboard axis. Все 51 396
объектов трёх корпусов декодированы; field 6 подтверждён executable, но не
наблюдается у точного типа `spNode`. Две PC-сцены используют дополнительную
четырёхбайтовую ID-only форму field 5. Полный разбор:
[`smo-class-sp-node.md`](../research/smo-class-sp-node.md).

`spRenderNode` имеет две секции: полный наследованный serializer `spNode` и
собственный повторяемый field 0 `esfRenderNodeRenderable`. Все 38 443 объекта и
53 057 renderable-связей трёх корпусов декодированы; targets ограничены
`spModel`, `spSkin`, `spParticleSystem` и `spLensFlare`. Полный разбор:
[`smo-class-sp-render-node.md`](../research/smo-class-sp-render-node.md).

`spModel` также имеет две секции: унаследованный `spRenderable` с material,
fog, `AlphaSortEnable` и `Priority`, затем собственные Base mesh и optional
Projection group. Все 118 720 объектов трёх корпусов строго декодированы и
распределены по семи вариантам присутствия полей; все связи разрешаются только
в `spMaterialData`, `spFog` и `spMeshData`. Полный разбор:
[`smo-class-sp-model.md`](../research/smo-class-sp-model.md).

`spSkin` продолжает эту цепочку третьей секцией: inherited material/fog/render
order, inherited base mesh/projection group и собственный `esfSkin`. Полностью
декодированы 1 758 объектов и 40 704 matrix slots; PC palette всегда имеет 16
слотов, PS2 — 64. Первое слово собственного поля является blend-influence hint,
а не reserved: ненулевые значения 1..4 совпадают с максимумом активных weights
на вершину. Полный разбор:
[`smo-class-sp-skin.md`](../research/smo-class-sp-skin.md).

## Partition, zones и visibility

| Hash | Класс | Объектов | Файлов | Назначение | Статус |
|---:|---|---:|---:|---|---|
| `0x912CC341` | `spPartitionSystem` | 29 | 29 | zones/portals/collisions + BSP/octree root | полный read-only decode PC/PS2 |
| `0x67672341` | `spPartitionNode` | 5 254 | 29 | узел partition tree | полный read-only decode PC/PS2 |
| `0x94BBCA2A` | `spPartitionRenderable` | 2 324 | 29 | ARGB debug color + 1..67 inline `spModel` | полный read-only decode PC/PS2 |
| `0x21A70829` | `spOctreeNode` | 731 | 11 | 8 indexed children + Pivot/Mins/Maxs | полный read-only decode PC/PS2 |
| `0x61254AB3` | `spZone` | 123 | 30 | sector/zone + 0..4 local partition roots | полный read-only decode PC/PS2 |
| `0x6523AC37` | `spZonePortal` | 206 | 18 | destination zone + quadrilateral + open flag | полный read-only decode PC/PS2 |
| `0xABB5AB2C` | `spZonePortalNode` | 103 | 18 | placed pair: BackToFront + FrontToBack portals | полный read-only decode PC/PS2 |
| `0x7362AB22` | `spBSPNode` | 108 | 18 | plane + две ветви binary partition tree | полный read-only decode PC/PS2 |
| `0x43D24430` | `spOcclusionVolume` | 20 | 8 | placed planar/closed visibility occluder geometry | полный read-only decode PC/PS2 |

Object-directory nesting этих классов не является transform hierarchy.
`spStaticRenderObject` содержит готовую world matrix, а partition/zone объекты
используются для spatial queries, culling и visibility.

`spZonePortal` является направленным ребром от zone физического родителя
`spPartitionNode` к собственному `DestinationZone`. Все наблюдаемые polygons —
четырёхугольники; у двух порталов одной пары вершины идут в точно обратном
порядке. Полный разбор: [spZonePortal](../research/smo-class-sp-zone-portal.md).

`spZonePortalNode` принадлежит `spPartitionSystem`, наследует самостоятельный
node-transform и в собственной секции хранит ровно две упорядоченные ссылки:
`BackToFront`, затем `FrontToBack`. Собственной polygon-геометрии у него нет.
Полный разбор:
[spZonePortalNode](../research/smo-class-sp-zone-portal-node.md).

`spBSPNode` наследует `spPartitionNode` и образует полное двоичное дерево:
каждый узел хранит slots 0/1, ведущие на inline `spBSPNode` либо reference
`spPartitionNode`, а собственная секция содержит нормализованную Plane.
Optional Polygon подтверждён обоими executable, но отсутствует во всём корпусе.
Полный разбор: [spBSPNode](../research/smo-class-sp-bsp-node.md).

`spOcclusionVolume` наследует `spNode`, а собственная секция содержит
обязательные portable IndexBuffer и VertexBuffer: triangle list с UInt16
индексами и position-only `Vector3` вершинами. Все наблюдаемые формы — связные
плоские диски с 4/5/6/10 вершинами; 54/60 строго выпуклые, шесть копий одного
десятиугольника имеют небольшую вогнутость. Все 20 PC/PS2-пар совпадают
побайтно. Полный разбор:
[spOcclusionVolume](../research/smo-class-sp-occlusion-volume.md).

## Навигация и BSP

| Hash | Класс | Объектов | Файлов | Назначение | Статус |
|---:|---|---:|---:|---|---|
| `0x188A161F` | `spNavigationGraph` | 27 | 27 | корень navigation graph и path table | полный read-only decode PC/PS2 |
| `0x7297173C` | `spMeshNavigationSet` | 119 | 27 | triangle graph, routing tables и mesh | полный read-only decode PC/PS2 |
| `0x385662AA` | `spNavigationPortal` | 70 | 16 | переход между navigation sets | полный read-only decode PC/PS2 |

`spMeshNavigationSet` наследует `spNode` и `spNavigationSet`. Его base-секция
хранит `NodeCount`, две плотные UInt32 routing matrices, byte-addressed ordered
adjacency, повторяемые portals и `Enabled`; собственная секция ссылается на
`spMeshBV`. Все 355 PC/PS2-экземпляров строго декодированы. Matrix cell выбирает
исходящее ребро, а out-of-degree значение 3 завершает либо помечает
недостижимый маршрут. Явный graph авторитетнее геометрии: на каждый corpus
snapshot найдены 32 authored links без точного общего ребра и четыре намеренно
отключённых shared-edge directions. Полный разбор:
[spMeshNavigationSet](../research/smo-class-sp-mesh-navigation-set.md).

`spNavigationPortal` хранит graph relation, ровно две endpoint-set relations,
повторяемые пары UInt8 node IDs и тройки source/destination/alternative index.
Все 8 044 membership-записи проверены против графов. `spNavigationGraph` хранит
sets, portals и квадратную row-major path table; все 2 507 строк и 2 344
alternative routes топологически подтверждены. Полные разборы:
[spNavigationPortal](../research/smo-class-sp-navigation-portal.md) и
[spNavigationGraph](../research/smo-class-sp-navigation-graph.md).

## Окружение и эффекты

| Hash | Класс | Объектов | Файлов | Назначение | Статус |
|---:|---|---:|---:|---|---|
| `0x7AC95AEC` | `spFog` | 413 | 413 | параметры fog | полный read-only decode PC/PS2 |
| `0x5E6402DF` | `spLightData` | 514 | 170 | тип, цвет и параметры light | полный read-only decode PC/PS2 |
| `0x5AFA1A4F` | `spParticleSystem` | 619 | 147 | emitter, lifetime, regions и render node | полный read-only decode PC/PS2 |
| `0x7A7124AF` | `spSkyBox` | 43 | 22 | sky/cloud/moon geometry | полный read-only decode PC/PS2 |
| `0x435370B5` | `spLensFlare` | 2 | 2 | lens-flare renderable | полный read-only decode PC/PS2 |

`spFog` встречается ровно в 413 из 416 PC SMO и 314 из 317 уникальных PS2 SMO.
Три одинаковых исключения — `Menus/gameover.smo`, `Menus/hud_training.smo` и
`Menus/object.smo`. Полный разбор вынесен в
[`smo-class-sp-fog.md`](../research/smo-class-sp-fog.md).

`spParticleSystem` полностью покрывает inherited `spRenderable`, fields 0..19 и
семь emission regions. `spSkyBox` владеет 1..3 inline models без face enum.
`spLensFlare` хранит material-backed elements/glare, occlusion и render node.
`spAnimTexController` хранит count, времена и равное число texture relations.
Подробности: [particles](../research/smo-class-sp-particle-system.md),
[sky box](../research/smo-class-sp-sky-box.md),
[lens flare](../research/smo-class-sp-lens-flare.md) и
[animated textures](../research/smo-class-sp-anim-tex-controller.md).

## Текст и GUI

| Hash | Класс | Объектов | Файлов | Назначение | Статус |
|---:|---|---:|---:|---|---|
| `0x52E86EFE` | `spTextNode` | 10 | 1 | GUI text node | полный PC read-only + PS2 executable |
| `0x19A745D7` | `spTextRenderable` | 10 | 1 | text renderable | полный PC read-only + PS2 executable |
| `0x4693490A` | `spFont` | 10 | 1 | font resource/reference | полный PC read-only + PS2 executable |

Эти классы найдены в одном PC menu SMO и образуют полностью декодированную
цепочку `spTextNode -> spTextRenderable -> spFont -> atlas`. Font содержит
height/baseline и 224 glyph records 0x20..0xFF; renderable — UTF-16LE text,
ARGB и optional wrap/alignment. В PS2 SMO экземпляров нет, но независимый PS2
executable подтверждает все три class/serializer. Полные разборы:
[spTextNode](../research/smo-class-sp-text-node.md),
[spTextRenderable](../research/smo-class-sp-text-renderable.md) и
[spFont](../research/smo-class-sp-font.md).

## Что не входит в эти 36 классов

`spAnimation` (`0x56EE563A`) находится в `.san`, который тоже является
FFPS-контейнером, но не SMO. Ещё 12 зарегистрированных классов не встречены ни в
одном SMO трёх корпусов: `spBoundingVolume`, `spCapsuleBV`,
`spCollisionManager`, `spCollisionMesh`, `spConvexBV`,
`spDXShadowMeshSerializer`, `spDXShadowVolumeManager`,
`spEnvironmentMapLayer`, `spMaterialTextureLayer`, `spPhysicsManager`,
`spShadowVolumeManager`, `spStdLayer`.

Это могут быть abstract bases, runtime managers или классы соседних форматов.
Они остаются в общем registry, но не выдаются за отсутствующие SMO-типы без
реального object-directory sample.

`spMaterialTextureLayer` и `spStdLayer` при этом уже восстановлены как runtime
классы по PC/PS2 executable; неизвестным остаётся только самостоятельный образец
в object directory корпуса.

## Воспроизведение

```text
SmoViewer.Inspect class-inventory <Media-directory> [--json]
```

Команда строго разбирает каждый SMO, агрегирует hash/name/object/file counts и
возвращает ненулевой код, если файл не разобрался или встретился class ID, которого
нет в `SmoClassRegistry`. Для `pc-working` текущий результат: `416/416`,
`177369 objects`, `36 known`, `0 unknown`. Полный результат schema v2 проверяется
командой `python research/audit_smo_corpus.py
local-data/results/smo-corpus-v2.sqlite`: `1 149/1 149`, `AUDIT PASS`.
