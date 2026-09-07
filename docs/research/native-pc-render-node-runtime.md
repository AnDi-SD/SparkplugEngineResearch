# PC render-узел: сцена, world cache и граница отрисовки

Checkpoint 7 ночного цикла 5/6 сентября 2026 года. PC-first, тот же pristine
SHA-256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [scene/world](native-pc-scene-world.md), не новый независимый путь.
Существующие original names сохраняются; новые имена методов математики явно
аналитические. PS2 сведения из старых карточек не расширялись.

## Размер, две таблицы и lifetime

Original factory `425520→13C5390` выделяет **1D4**; ctor `424F60→13D8030`
вызывает Node, затем support constructor `469E00→13E0CB0`. Primary `6DCAA4`
содержит14 callable slots, secondary `6DCADC` —6. Прежние «20 primary» были
ошибкой нашей интерпретации соседних таблиц, не повреждением executable.

| PC offset | Подтверждённая роль/default |
|---:|---|
| B4 / B8 / BC-C4 | support vptr; untouched allocator; owned renderable vector |
| C8 / D8 | local/world sphere: center3+radius, initially zero |
| E8 / EC | pointers to own matrices138/178 |
| F0-117 | light-cache support, initially zero; полный тип/внутренние поля открыты |
| 118 / 11C | zero words;118 сравнивается с scene40, original roles не названы |
| 120-123 | bytes00,01,01,01;123 разрешает light-manager update |
| 124 / 128 / 12C | complete-object self / previous / next render-node in scene |
| 130 / 131-133 / 134 | cull bypass0 / untouched padding / matrix dirty0 |
| 138 / 178 / 1B8 | identity world/inverse matrices / reciprocal world scale(1,1,1) |
| 1C4 / 1C8-1D0 | untouched allocator / borrowed callback pointer vector |

Deleting4255D0→dtor425050: notify/drain callbacks424DD0(1), free callback vector,
support469C80, Node422150. Support dtor clears renderer75DB68+C190 **только**
если cache pointer совпадает с nodeF0 и renderer byteC9C4 равен0. Зависимость
от существующего renderer storage реальна; для probes дана явная zero-storage
fixture, не fake renderer constructor и не Direct3D device.

## Сценовый render-list — intrusive и невладеющий

Scene18=head,1C=tail,20=count; links находятся в самих render-узлах128/12C.
`45A970` при IsKindOf(RenderNode603625D0) вызывает append
`45A730→45B010`. Пустой/непустой список обновляется без нового intrusive ref:
владение остаётся у parent child-list. `45AB30` вызывает unlink
`45A630→13D00A0` с receiver scene18. Head/tail/neighbors меняются, links
обнуляются; underflow-countFFFFFFFF сбрасывается0. Сам list helper не ищет
membership и не защищает от duplicate/foreign node — такие повреждающие
входы в probe не запускаются и не объявляются поддержанными.

Original Attach421A60/reparent между двумя scenes проверены для three render
nodes и вложенного render-descendant под plain Node: порядок, middle/head/tail
unlink, same-parent no-op и native teardown. Правило об общей сцене нельзя
заменять только записью parent pointer.

В45A810/45A8C0 статически подтверждён **exact class ID**, не IsKindOf switch:
SkyBox7A7124AF обрабатывается scene30 и возвращается до renderable traversal;
LensFlare435370B5 идёт в scene28 v2C/v30; Projection IDs1CCA7732,32BB2F56,
750F73D9 и **58DA4026** — в scene2C v24/v28. Последний ID не найден в
registered type catalog, поэтому не назван догадочным классом. Эти specialized
manager algorithms, Light/Partition/Occlusion ещё не объявляются исполненными
данным checkpoint; это следующий обязательный dependency slice.

## World, bounds и lazy matrices

Original world virtual30=`4250F0` сначала сохраняет `oldFlags|inherited`, потом
вызывает Node421420 (включая descendants). После этого всегда обновляет
reciprocal world scale1B8. Billboard masks300000 ставят node dirty bit1 **после**
capture: чистый billboard подготавливает следующий кадр, не текущую матрицу.

При captured bit2 support469820 сбрасывает только local radius, не center,
обходит renderable v2C sphere getters, объединяет через463670. Tiny incoming
radius<.001 игнорируется; containing sphere заменяет/сохраняет результат;
near-coincident center comparison покоординатный. Helper преобразует sphere
**старой cached matrix138**, измеряя длину преобразованного X-radius.

При captured bit1 world sphere затем перезаписывается из current world PRS:
center=`(localCenter*worldScale)*worldOrientation+worldPosition`, radius=
`localRadius*max(abs(worldScale))`; matrixDirty134|=1. Поэтому bounds-only и
transform-dirty paths **не эквивалентны** при nonuniform scale. Если scene
существует, byte123 и Enabled200 разрешают46AC40 light-cache rebuild;424EF0
обновляет partition membership, кроме skybox/self-partition/disabled cases.
Последние nonempty manager callbacks — ещё внешняя граница.

Support method4248D0 вычисляет matrices138/178 лишь при dirty134 bit1:
461D70 builds affine PRS;461EB0 использует transpose orientation и reciprocal
scale, не general inverse sheared matrix. Dirty снимается **до** backend call.
Renderer secondary18 slot14 получает `(worldMatrix,inverseMatrix)`. При false
матрицы уже обновлены, но sphere в rendererC9C8 не копируется; при nonzero
копируется и возвращается normalized true. Cache pointerC190 публикуется до
backend result, если !C9C4. Это проверенные failure semantics, не rollback.

## Cull, direct draw и queue

424840 возвращает «отсечён» при любой из шести plane comparisons:
`nx*x+nz*z+ny*y-plane.d < -radius`. Planes берутся camera1C4 с шагом16.
Касание остаётся видимым. Byte130 либо radius<=float32(.001) **пропускают cull**,
а не автоматически скрывают узел. Nonfinite inputs в переносимом срезе не
считаются описанными этим конечным набором исследований.

Secondary424B60 получает `(camera,forceVisible)` и complete this+B4:

- !Enabled200 или culled →success без matrix/draw;
- rendererC050=0: собственный support v4 готовит matrices, failure останавливает;
  каждый renderable v24 получает `(camera,support)`, его return игнорируется;
- rendererC050!=0: каждый идёт в456310 `(renderable,support,camera)`; первый false
  останавливает проход; immediate matrix setup здесь не вызывается.

Renderer queue внутренности, material pre/post и mesh submission не заменены
возвращающими success заглушками в исходнике: leaf seams только записывают
границу original probe.

## Callback-vector semantics

424D60(callback,notify) находит первое совпадение, переносит последний pointer
на его место, уменьшает end, затем optionally вызывает callback v2C(node,0).
Missing pointer — no-op.424DD0(true) идёт с конца, после callback перечитывает
begin/end и ещё раз уменьшает непустой список; capacity не освобождает.
424DD0(false) освобождает/обнуляет vector без notifications. Если explicit leaf
сам снимает last record, native дополнительный pop может пропустить другой
callback: это проверено на bounded fixture, **не обещание reentry safety**.

## Перенос и проверки

`Analysis/PC/spRenderNodeMath.h` восстанавливает finite-input sphere union,
cached/PRS sphere, inverse PRS и cull formulas. Это независимый исполняемый
срез. На checkpoint7 class source ещё представлял ownership facade;
[checkpoint10](native-pc-model-render-world.md) уже подключил geometry getters,
virtual world/lazy caches/explicit light binding и исправил clone ownership.
Полный automatic scene/partition register/callback loop остаётся открытым.

- `probe_pc_scene_render_registry.py`: **55/55** original ctor/registry/lifetime;
- `probe_pc_render_node_runtime.py`: **54/54** original world/caches/cull/draw gates;
- `probe_pc_render_node_callbacks.py`: **15/15** original remove/drain/lifetime;
- `inspect_pc_render_node.py`: **21/21** PC-only SHA/table/call anchors;
- `SparkplugRenderNodeTests`: **11/11**; CTest **13/13**;
- `compare_pc_render_node.py`: **1504/1504**,144 native/portable finite cases,
  absolute/relative tolerance3e-5; не bit-identical x87 claim.

Все tracked original allocations освобождены. Literal geometry objects/planes,
preinitialized RTTI parent records, renderer storage/device leaves названы
явно. Common100k instructions/2s call,30s child; никаких Windows/GPU/game calls,
PS2 исполнения, изменений assets/apps или публикации. Первый scout без renderer
storage остановился на null dependency в support dtor; исправлена fixture,
не original EXE. IDA misaligned protected-return disassembly проверялась raw
Capstone decoding с фактической entry address, а не подгонкой оригинальных bytes.
