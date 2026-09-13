# Типы объектов SMO

После завершения очереди все 36 встреченных классов имеют строгий structural/read-only decoder. Статус «полный read-only» означает полный разбор наблюдаемого layout, но не гарантирует безопасную запись или знание всей runtime-семантики.

| Hash | Класс | Назначение | Статус |
| ---: | --- | --- | --- |
| `0x763277DB` | `spModel` | mesh/material/fog и render-order binding | полный read-only decode PC/PS2 |
| `0x695C0F65` | `spNode` | transform, hierarchy, bones и service nodes | полный read-only decode PC/PS2 |
| `0x603625D0` | `spRenderNode` | renderable node и связь с visual resources | полный read-only decode PC/PS2 |
| `0x56D67170` | `spStaticRenderObject` | статическое world-space размещение уровня | полный read-only decode PC/PS2 |

## Геометрия, материалы и текстуры

| Hash | Класс | Назначение | Статус |
| ---: | --- | --- | --- |
| `0x33C34CF0` | `spMeshData` | vertex/index buffers и mesh bounds | полный структурный read-only decode PC/PS2 |
| `0x6160348B` | `spMaterialData` | material state, passes и texture/controller links | полный структурный read-only decode PC/PS2 |
| `0x78EA082B` | `spTextureData` | встроенные raster textures | полный структурный read-only decode PC/PS2 |
| `0x681F2043` | `spSkin` | renderable/model state, bone palette и inverse-bind matrices | полный read-only decode PC/PS2 |
| `0x1C0053D6` | `spUVController` | управление UV transform/animation | полный read-only decode PC/PS2 |
| `0x4C633E85` | `spMaterialColorController` | пять material evaluator-секций | полный read-only decode PC + PS2 executable verification |
| `0x16FB0E47` | `spAnimTexController` | временная последовательность texture frames | полный read-only decode PC/PS2 |

`spUVController` содержит один вложенный `spTransFunctionEval`: translation XYZ, scale XYZ, rotation, UV pivot и rotation axis. Одиннадцать размеров являются разреженными формами с опущенными default-полями, а не подтипами.

## Collision и bounding volumes

| Hash | Класс | Назначение | Статус |
| ---: | --- | --- | --- |
| `0x47A97C0E` | `spCollisionInfo` | collision owner, group, transform и primitive relationship | полный read-only decode PC/PS2 |
| `0x3F453DE7` | `spMeshBV` | triangle collision mesh + optional wxFaceData | полный read-only decode PC/PS2 |
| `0x4DA04889` | `spOBBBV` | oriented box bounding volume | полный read-only decode PC/PS2 |
| `0x7B4C0876` | `spBoxBV` | axis-aligned box bounding volume | полный read-only decode PC/PS2 |
| `0x390946D2` | `spSphereBV` | spherical bounding volume | полный read-only decode PC/PS2 |

Для `spOBBBV` field 1 хранит полный размер `(X,Y,Z)`, а не half-extents.
Необязательные field 0 position и field 2 quaternion rotation подтверждены
кодом обеих платформ, хотя доступные SMO их не сериализуют. Полный разбор:
[`smo-class-sp-obbbv.md`](../reference/classes/sp-obbbv.md).

`spSphereBV` имеет optional field 0 position и обязательный field 1 radius.
Единственный `SFX/vase.smo` использует radius `92.0507889`, одинаковый на PC и
PS2. Полный разбор:
[`smo-class-sp-sphere-bv.md`](../reference/classes/sp-sphere-bv.md).

`spSkin` продолжает эту цепочку третьей секцией: inherited material/fog/render
order, inherited base mesh/projection group и собственный `esfSkin`. Полностью
декодированы 1 758 объектов и 40 704 matrix slots; PC palette всегда имеет 16
слотов, PS2 — 64. Первое слово собственного поля является blend-influence hint,
а не reserved: ненулевые значения 1..4 совпадают с максимумом активных weights
на вершину. Полный разбор:
[`smo-class-sp-skin.md`](../reference/classes/sp-skin.md).

## Partition, zones и visibility

| Hash | Класс | Назначение | Статус |
| ---: | --- | --- | --- |
| `0x912CC341` | `spPartitionSystem` | zones/portals/collisions + BSP/octree root | полный read-only decode PC/PS2 |
| `0x67672341` | `spPartitionNode` | узел partition tree | полный read-only decode PC/PS2 |
| `0x94BBCA2A` | `spPartitionRenderable` | ARGB debug color + 1..67 inline `spModel` | полный read-only decode PC/PS2 |
| `0x21A70829` | `spOctreeNode` | 8 indexed children + Pivot/Mins/Maxs | полный read-only decode PC/PS2 |
| `0x61254AB3` | `spZone` | sector/zone + 0..4 local partition roots | полный read-only decode PC/PS2 |
| `0x6523AC37` | `spZonePortal` | destination zone + quadrilateral + open flag | полный read-only decode PC/PS2 |
| `0xABB5AB2C` | `spZonePortalNode` | placed pair: BackToFront + FrontToBack portals | полный read-only decode PC/PS2 |
| `0x7362AB22` | `spBSPNode` | plane + две ветви binary partition tree | полный read-only decode PC/PS2 |
| `0x43D24430` | `spOcclusionVolume` | placed planar/closed visibility occluder geometry | полный read-only decode PC/PS2 |

Object-directory nesting этих классов не является transform hierarchy.
`spStaticRenderObject` содержит готовую world matrix, а partition/zone объекты
используются для spatial queries, culling и visibility.

`spZonePortal` является направленным ребром от zone физического родителя
`spPartitionNode` к собственному `DestinationZone`. Все наблюдаемые polygons —
четырёхугольники; у двух порталов одной пары вершины идут в точно обратном
порядке. Полный разбор: [spZonePortal](../reference/classes/sp-zone-portal.md).

`spZonePortalNode` принадлежит `spPartitionSystem`, наследует самостоятельный
node-transform и в собственной секции хранит ровно две упорядоченные ссылки:
`BackToFront`, затем `FrontToBack`. Собственной polygon-геометрии у него нет.
Полный разбор:
[spZonePortalNode](../reference/classes/sp-zone-portal-node.md).

`spOcclusionVolume` наследует `spNode`, а собственная секция содержит
обязательные portable IndexBuffer и VertexBuffer: triangle list с UInt16
индексами и position-only `Vector3` вершинами. Все наблюдаемые формы — связные
плоские диски с 4/5/6/10 вершинами; 54/60 строго выпуклые, шесть копий одного
десятиугольника имеют небольшую вогнутость. Все 20 PC/PS2-пар совпадают
побайтно. Полный разбор:
[spOcclusionVolume](../reference/classes/sp-occlusion-volume.md).

## Навигация и BSP

| Hash | Класс | Назначение | Статус |
| ---: | --- | --- | --- |
| `0x188A161F` | `spNavigationGraph` | корень navigation graph и path table | полный read-only decode PC/PS2 |
| `0x7297173C` | `spMeshNavigationSet` | triangle graph, routing tables и mesh | полный read-only decode PC/PS2 |
| `0x385662AA` | `spNavigationPortal` | переход между navigation sets | полный read-only decode PC/PS2 |

`spMeshNavigationSet` наследует `spNode` и `spNavigationSet`. Его base-секция
хранит `NodeCount`, две плотные UInt32 routing matrices, byte-addressed ordered
adjacency, повторяемые portals и `Enabled`; собственная секция ссылается на
`spMeshBV`. Все 355 PC/PS2-экземпляров строго декодированы. Matrix cell выбирает
исходящее ребро, а out-of-degree значение 3 завершает либо помечает
недостижимый маршрут. Явный graph авторитетнее геометрии: в известных данных
snapshot найдены 32 authored links без точного общего ребра и четыре намеренно
отключённых shared-edge directions. Полный разбор:
[spMeshNavigationSet](../reference/classes/sp-mesh-navigation-set.md).

## Окружение и эффекты

| Hash | Класс | Назначение | Статус |
| ---: | --- | --- | --- |
| `0x7AC95AEC` | `spFog` | параметры fog | полный read-only decode PC/PS2 |
| `0x5E6402DF` | `spLightData` | тип, цвет и параметры light | полный read-only decode PC/PS2 |
| `0x5AFA1A4F` | `spParticleSystem` | emitter, lifetime, regions и render node | полный read-only decode PC/PS2 |
| `0x7A7124AF` | `spSkyBox` | sky/cloud/moon geometry | полный read-only decode PC/PS2 |
| `0x435370B5` | `spLensFlare` | lens-flare renderable | полный read-only decode PC/PS2 |

`spFog` встречается ровно в 413 из 416 PC SMO и 314 из 317 уникальных PS2 SMO.
Три одинаковых исключения — `Menus/gameover.smo`, `Menus/hud_training.smo` и
`Menus/object.smo`. Полный разбор вынесен в
[`smo-class-sp-fog.md`](../reference/classes/sp-fog.md).

`spParticleSystem` полностью покрывает inherited `spRenderable`, fields 0..19 и
семь emission regions. `spSkyBox` владеет 1..3 inline models без face enum.
`spLensFlare` хранит material-backed elements/glare, occlusion и render node.
`spAnimTexController` хранит count, времена и равное число texture relations.
Подробности: [particles](../reference/classes/sp-particle-system.md),
[sky box](../reference/classes/sp-sky-box.md),
[lens flare](../reference/classes/sp-lens-flare.md) и
[animated textures](../reference/classes/sp-anim-tex-controller.md).

## Текст и GUI

| Hash | Класс | Назначение | Статус |
| ---: | --- | --- | --- |
| `0x52E86EFE` | `spTextNode` | GUI text node | полный PC read-only + PS2 executable |
| `0x19A745D7` | `spTextRenderable` | text renderable | полный PC read-only + PS2 executable |
| `0x4693490A` | `spFont` | font resource/reference | полный PC read-only + PS2 executable |

Эти классы найдены в одном PC menu SMO и образуют полностью декодированную
цепочку `spTextNode -> spTextRenderable -> spFont -> atlas`. Font содержит
height/baseline и 224 glyph records 0x20..0xFF; renderable — UTF-16LE text,
ARGB и optional wrap/alignment. В PS2 SMO экземпляров нет, но независимый PS2
executable подтверждает все три class/serializer. Полные разборы:
[spTextNode](../reference/classes/sp-text-node.md),
[spTextRenderable](../reference/classes/sp-text-renderable.md) и
[spFont](../reference/classes/sp-font.md).

## Что не входит в эти 36 классов

Это могут быть abstract bases, runtime managers или классы соседних форматов.
Они остаются в общем registry, но не выдаются за отсутствующие SMO-типы без
реального object-directory sample.

```text
SmoViewer.Inspect class-inventory <Media-directory> [--json]
```
