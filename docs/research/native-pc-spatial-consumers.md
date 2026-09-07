# PC BSP/Octree: static и occlusion consumers

Checkpoint20, 6 сентября2026. Продолжение [BSP](native-pc-bsp-runtime.md),
[Octree](native-pc-octree-runtime.md) и [ownership](native-pc-partition-runtime.md).
Подтверждены ранее только статически намеченные nonempty registration paths.
Partial C++ classes этого checkpoint не расширяются до полной Scene.

## StaticRenderObject — различие двух деревьев

Оба принимают complete `spStaticRenderObject`, worldSphere по38..44,
но их slot58 **не идентичен**:

| Условия | BSP480AD0 | Octree449A90 |
|---|---|---|
| Zone60 отсутствует | sphereMask48 → все выбранные children | sphereMask48 → все выбранные children |
| Zone60 есть, sphere не касается split | единственный child по signed distance | прежний sphereMask48 |
| Zone60 есть, sphere касается split | append в **текущий BSP**426740 | прежний sphereMask48, даже при нескольких children |

Изначальное краткое описание BSP «Static использует mask48» было неполным:
оно верно только для no-Zone ветви. Теперь уточнено по complete480AD0 и
шести actual cases на каждое дерево. Sphere(.7,.7,.9,r1) в Octree получает
original F0 shortcut, даже для Static; не заменяется идеальным overlap.

Leaf426740 retains intrusive reference и вызывает actual vector append426340.
Каждое повторное добавление создаёт ещё одну запись и ещё один reference;
дедупликации тут нет. После append field88 Static получает Scene из root80.
Original recursive graph destruction освобождает все ссылки, включая
несколько разных leaves и duplicates, затем единственный объект.

## OcclusionVolume — borrowed reciprocal links

BSP480630/Octree449D30 читают worldSphere1A4 и flagsB0 самого occlusion node:
Zoned dynamic touching →current root, non-touching →one child;
static400 или no Zone →sphereMask48/all selected children.

В отличие от RenderNode, **нет billboard300000 исключения**: static+100000
идёт по обычному static пути. Проверены также cleared Enabled200 — эти
lower-level insertion functions его не проверяют. Это не обобщение на все
upstream call sites или игровой жизненный цикл.

Root40 и OcclusionB4 — reciprocal **borrowed** vectors, refs не растут.
Actual46DCF0(notify=1) перед повторным размещением очищает обе стороны.
Исполнены factory470A70 и original empty-geometry destruction. WorldSphere
задан как явный вход; **не** full Init470FE0, не silhouette generation и
не доказательство successful authored Occlusion load.

## Visibility dedup — отдельная Debug21 проверка

Whole Scene45EC70 исполнен для обоих деревьев по **оригинальной unclipped
Debug21 ветке**. Один Static дважды registered: BSP имеет две записи в
текущем root, Octree — по две записи в двух leaves, четыре owning references.
На каждом из двух frames support draw вызывается ровно один раз, Static78
совпадает с Scene40 stamp; следующий frame снова рисует один раз.

Decoded graph assignment и sphere — fixture inputs. Первая static-only
попытка обнаружила ещё не созданный DebugManager; fresh probe исполнил
original lazy factory41E380 и установил global75526C, как native callers.
Это lazy reference, не собственный singleton755278; различие уже описано в
[карточке DebugManager](native-class-sp-debug-manager.md).
Нулевая страница не мапилась и memory guard не ослаблялся. Original cleanup
освобождает manager и duplicate Static references.

Это **не закрывает** normal clipped traversal45E870: protected copy ни разу
не возобновлялся/заменялся, лимит100k instructions/2sec и child30sec прежний.
Никаких GPU pixels/OS/game validation и изменений игровых файлов.

## Проверки / оставшиеся неизвестные

- `python research/probe_pc_spatial_consumers.py`: **94/94**, шесть fresh
  children: BSP static21/occlusion20/debugScene6, Octree21/20/6;
- `python research/inspect_pc_spatial_consumers.py`: **15/15** pristine,
  complete Octree Zone-free method, BSP Zone gate/base append, Occlusion
  static flags и intrusive/static Scene publication anchors;
- Portable source не изменялся; предыдущий full **CTest21/21** остаётся
  проверкой тех же исходников, а не новой реализации этих consumers.

Open: nonempty Collision insertion и его owner lifetime, BSP64..7C consumers,
optional split polygon producers, full normal recursive clipping/Visibility
constructor/Occlusion Init, portable registration/source integration.
