# PC BSP/Octree: static и occlusion consumers

## StaticRenderObject — различие двух деревьев

Оба принимают complete `spStaticRenderObject`, worldSphere по38..44,
но их slot58 **не идентичен**:

| Условия | BSP480AD0 | Octree449A90 |
| --- | --- | --- |
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

Root40 и OcclusionB4 — reciprocal **borrowed** vectors, refs не растут.
Actual46DCF0(notify=1) перед повторным размещением очищает обе стороны.
Исполнены factory470A70 и original empty-geometry destruction. WorldSphere
задан как явный вход; **не** full Init470FE0, не silhouette generation и
не доказательство successful authored Occlusion load.

Whole Scene45EC70 исполнен для обоих деревьев по **оригинальной unclipped
Debug21 ветке**. Один Static дважды registered: BSP имеет две записи в
текущем root, Octree — по две записи в двух leaves, четыре owning references.
На каждом из двух frames support draw вызывается ровно один раз, Static78
совпадает с Scene40 stamp; следующий frame снова рисует один раз.

## Границы описания

Open: nonempty Collision insertion и его owner lifetime, BSP64..7C consumers,
optional split polygon producers, full normal recursive clipping/Visibility
constructor/Occlusion Init, portable registration/source integration.
