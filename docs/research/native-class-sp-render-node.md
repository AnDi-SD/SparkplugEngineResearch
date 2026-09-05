# Нативный класс `spRenderNode`

Дата проверки: 2026-09-05. Статус: class identity, direct base, factory/clone,
renderable ownership, optimizer traversal и PS2 layout подтверждены бинарно.
PC tail и исходные имена render/cull API пока остаются неизвестными.

Этот документ описывает runtime-класс. Формат его SMO-полей разобран отдельно
в [`smo-class-sp-render-node.md`](smo-class-sp-render-node.md); wire layout нельзя
выдавать за layout C++-объекта.

| Факт | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / direct base | `0x603625D0 / spNode` | same |
| Registration / initializer | `0x0075E150 / 0x006D2A20` | `0x004AB290 / 0x00483F00` |
| Factory | protected entry `0x00425520` | `0x001AB160` |
| Constructor | protected | `0x001AAFF0` |
| Destructor | deleting `0x004255D0` | `0x001AAEF0` |
| Clone / copy | `0x00425580 / 0x00424980` | `0x001AB090 / 0x001AA230` |
| Registration getter | `0x00425030` | `0x001A9DE0` |
| Primary vtable | `0x006DCAA4` | header `0x00490370` |

Точного исходного пути класса в строках executable нет. Известен только
`Z:\Sparkplug\Code\Sparkplug\spRenderNodeSerializer.cpp`, поэтому нынешние
`Code/Sparkplug/spRenderNode.*` явно помечены как inferred paths.

## Layout и владение renderables

На PC защищённая factory скрыта SecuROM-переходом. Независимые обращения
`spSceneGraphOptimizer::OptimizeNode` фиксируют vector prefix после полного
`spNode` размером `0xB4`:

| Offset | Роль |
|---:|---|
| `+0xB4` | поле с пока неизвестной ролью |
| `+0xB8` | allocator/служебное слово compiler-specific vector |
| `+0xBC` | begin массива renderable-ссылок |
| `+0xC0` | end |
| `+0xC4` | capacity end |

Это лишь доказанный префикс `0xC8`, не полный PC `sizeof`.

PS2 factory чисто выделяет `0x1E0` байт с выравниванием 16. Там `spNode`
занимает `0xC0`, а renderable-контейнер принадлежит support-subobject по
`+0xC8`: count расположен по `+0xD0`, storage по `+0xD4`. Constructor также
создаёт два matrix/cache блока по `+0x140/+0x180`, пишет self-link `+0x134`,
inverse scale `(1,1,1)` по `+0x1C0` и инициализирует хвостовой callback state.
Exact структура сохранена в `Analysis/PS2/SparkplugAbi.h`; PC и PS2 layouts
намеренно не объединены.

Native список владеет renderables через intrusive references. Copy/clone
проходит по нему и связывает либо клонирует элементы через общий clone manager;
повторная ссылка не обязана создавать второй объект. Destructor сначала
освобождает хвостовые callbacks, затем support-subobject и лишь потом `spNode`.

Portable реконструкция использует `shared_ptr`, сохраняет порядок, допускает
повторные ссылки, при clone сохраняет их aliasing и инвалидирует безопасный
bounds-state при attach/detach. Это host-ownership, а не попытка повторить ABI.

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

PC primary vtable по `0x006DCAA4` имеет 20 entries; её точный список и обе
PS2 таблицы контролирует `research/inspect_render_node.py`.

## Связь с optimizer и открытая граница

PC `spSceneGraphOptimizer::OptimizeNode` (`0x004C19D0`) проверяет RTTI
`0x603625D0`, проходит диапазон `+0xBC..+0xC0`, выбирает объекты
`spModel` (`0x763277DB`), вызывает model callback и затем рекурсивно обходит
children. Это независимо подтверждает назначение списка и ставит
`spRenderNode` между общим scene graph и DX batch/materialization.

Открыты: original header/TU/API, полный PC размер и tail, исходное имя
support-subobject, callback record layout, cached transform/cull state,
точные сигнатуры bounds/update/render slots и отношение этих кэшей к scene
manager. Optimizer и serializer уже разобраны до своих защищённых/fixup
границ; следующий обязательный узел render-пути — material/fog state внутри
pre/post-render.
