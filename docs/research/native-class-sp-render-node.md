# Нативный класс `spRenderNode`

Дата проверки: 2026-09-06. Статус: class identity/direct base, полный PC layout,
native factory/lifetime/scene registration, world/bounds/matrix/cull/draw boundary
проверены original instructions. [Новый PC runtime срез](native-pc-render-node-runtime.md)
отделяет исполняемые доказательства и переносимую математику от ещё не готовой
полной runtime-реконструкции класса. Исходные имена render/cull API не найдены.

Этот документ описывает runtime-класс. Формат его SMO-полей разобран отдельно
в [`smo-class-sp-render-node.md`](smo-class-sp-render-node.md); wire layout нельзя
выдавать за layout C++-объекта.

| Факт | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / direct base | `0x603625D0 / spNode` | same |
| Registration / initializer | `0x0075E150 / 0x006D2A20` | `0x004AB290 / 0x00483F00` |
| Factory | protected entry `0x00425520` | `0x001AB160` |
| Constructor | `424F60→13D8030` | `0x001AAFF0` |
| Destructor | deleting `0x004255D0` | `0x001AAEF0` |
| Clone / copy | `0x00425580 / 0x00424980` | `0x001AB090 / 0x001AA230` |
| Registration getter | `0x00425030` | `0x001A9DE0` |
| Primary vtable | `0x006DCAA4` | header `0x00490370` |

Точного исходного пути класса в строках executable нет. Известен только
`Z:\Sparkplug\Code\Sparkplug\spRenderNodeSerializer.cpp`, поэтому нынешние
`Code/Sparkplug/spRenderNode.*` явно помечены как inferred paths.

## Layout и владение renderables

PC factory `425520→13C5390` теперь исполнена и выделяет **0x1D4**. После
полного `spNode` (`0xB4`) находится secondary support:

| Offset | Роль |
|---:|---|
| `+0xB4` | secondary vptr `6DCADC`, шесть методов, original type name неизвестен |
| `+0xB8` | allocator/служебное слово compiler-specific vector |
| `+0xBC` | begin массива renderable-ссылок |
| `+0xC0` | end |
| `+0xC4` | capacity end |

Старый prefix type `spRenderNodeObservedPrefixLayout` оставлен для совместимости,
но больше не является пределом знания. Новый `spRenderNodeLayout` покрывает
local/world spheres `C8/D8`, matrix pointers `E8/EC`, light-cache `F0`, self124,
scene links128/12C, cull bypass130, dirty134, matrices138/178, inverse scale1B8
и callback vector1C4. Неназванные cache words/bytes и untouched padding не
получают вымышленных ролей. Exact fields/static asserts — `Analysis/PC/SparkplugAbi.h`.

PS2 factory чисто выделяет `0x1E0` байт с выравниванием 16. Там `spNode`
занимает `0xC0`, а renderable-контейнер принадлежит support-subobject по
`+0xC8`: count расположен по `+0xD0`, storage по `+0xD4`. Constructor также
создаёт два matrix/cache блока по `+0x140/+0x180`, пишет self-link `+0x134`,
inverse scale `(1,1,1)` по `+0x1C0` и инициализирует хвостовой callback state.
Exact структура сохранена в `Analysis/PS2/SparkplugAbi.h`; PC и PS2 layouts
намеренно не объединены.

Native список владеет renderables через intrusive references. PC copy/clone
для **каждого вхождения** вызывает always-clone `412BE0`: повторные указатели
дают разные Model, но shared Mesh. Это не map-aware entry `412C40→4D3810`.
Copy **добавляет** элементы в непустой destination, не очищает его; spheres
копируются до и снова после append-loop. Destructor сначала
освобождает хвостовые callbacks, затем support-subobject и лишь потом `spNode`.

Portable реконструкция использует `shared_ptr`, сохраняет порядок и разные
Model для повторов при clone. Attach немедленно пересчитывает bounds, как
native `469ED0→469820`, **без установки dirty bits**. Host detach/clear не
выдаются за ещё не исполненный native individual detach469F50.

## Bounds, update и render boundary

Чистые PS2 bodies показывают больше, чем один контейнер:

- `0x001AACA0` собирает общие bounds, вызывая virtual `+0x38` каждого
  renderable;
- `0x001AABF0` передаёт update всем renderables через их slot `+0x24`;
- `0x001AAA20` обновляет cached transform и вызывает renderer world-matrix
  operation `14`;
- `0x001AA810` проверяет node mask `0x200`, выполняет sphere/frustum-plane
  culling по cached center/radius и dispatch-ит чистый render-slot каждого
  прошедшего `spRenderable`;
- `0x001AA580` является отдельным debug-overlay path;
- рекурсивная node-mask `0x200` остаётся общей с `spNode`.

`spModel` в этом dispatch затем передаёт `baseMesh` в renderer slot `9`.
Таким образом, доказана цепочка `camera matrices -> RenderNode culling ->
spRenderable render -> spModel -> platform mesh backend`. Названия secondary
interface, точные сигнатуры и часть cache fields ещё не найдены, поэтому
portable код не симулирует renderer.

PS2 primary vtable header содержит 16 подтверждённых slots:

`0, 0, 1AAEF0, 1A5AC0, 1AB090, 1AA230, 1A9DE0, 100010, 100050,
1AAEA0, 1AACA0, 1AABF0, 1A7160, 1A7130, 1AA2C0, 1A9F30`.

Secondary header содержит 13 slots:

`0, 0, 1AB2D0, 135E10, 135E00, 1AB2E0, 135E80, 135E70, 1AAA20,
1AA810, 1AA580, 1A9F00, 1A9DF0`.

Исправлена ошибка прежней карточки: PC primary по `0x006DCAA4` имеет **14**
entries; соседние **6** относятся к secondary `6DCADC` с `this+=B4`. Это не
20 методов одной таблицы. PC-only anchors — `research/inspect_pc_render_node.py`;
старый cross-platform inspector также исправлен. PS2 в новом цикле не исследуется.

## Связь с optimizer и открытая граница

PC `spSceneGraphOptimizer::OptimizeNode` (`0x004C19D0`) проверяет RTTI
`0x603625D0`, проходит диапазон `+0xBC..+0xC0`, выбирает объекты
`spModel` (`0x763277DB`), вызывает model callback и затем рекурсивно обходит
children. Это независимо подтверждает назначение списка и ставит
`spRenderNode` между общим scene graph и DX batch/materialization.

Checkpoint10 [Model→RenderNode world](native-pc-model-render-world.md) перенёс
virtual world-dispatch, native geometry getters, local/world sphere и lazy
world/inverse caches в классы; light-cache обновляется через explicit manager
binding. Clone **не копирует** cached matrices/dirty134/light-cache/manager
binding, но копирует spheres, controls120..123 и cull130. Native ownership29,
portable34, сквозное сравнение2272/32 сценария; прежние math1504 также проходят.

Открыты: original header/TU/API, имя support-subobject и light-cache helper,
remaining cache roles, automatic Scene/Partition/Occlusion side effects,
scene45EC70/renderer456310/material submission и native individual detach.
Простые callback-vector records теперь
подтверждены как borrowed pointers с swap-last/remove/drain протоколом, но их
оригинальные interface names/внешние lifetime invariants ещё не закрыты.
