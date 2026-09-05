# Нативный класс `spModel`

Дата проверки: 2026-09-05. Статус: проверяемый ownership/state slice;
model-to-renderer draw path подтверждён на PC/PS2, PS2 classifier остаётся
evidence-only.

## Идентичность

`spModel` имеет class ID `0x763277DB`, direct base `spRenderable`
(`0x4FDA4542`) и factory на обеих платформах. Исходный путь не найден;
созданный `Sparkplug/Code/Sparkplug/spModel.*` явно inferred.

| Факт | PC | PS2 |
|---|---:|---:|
| registration | `0x00760CF8` | `0x004A8D40` |
| initializer | `0x006D3EE0` | `0x00482380` |
| factory | `0x00479ED0` (`.rld` thunk) | `0x0015AB90` |
| registration getter | `0x00479A80` | `0x00159F70` |
| destructor | `0x00479A90` | `0x0015A9F0` |
| deleting destructor | `0x00479F90` | ABI slot `0x00100810` |
| clone | `0x00479F40` | `0x0015AAB0` |
| copy | `0x00479E60` | `0x0015A4D0` |
| base-mesh setter | `0x00479E20` | `0x0015A5A0` |
| render body | `0x00479DC0` | `0x0015A640` |
| vtable | `0x006EAA58` | `0x0048E250` |
| observed/exact extent | `0x60` | `0x58` |

PC initializer буквально передаёт `(class=0x763277DB, base=0x4FDA4542,
name=0x006EAAA0, baseRegistration=0x0075E030, factory=0x00479ED0)`.
PS2 factory выделяет exact `0x58`; PC `+0x58/+0x5C` accesses доказывают
минимальный полный extent `0x60`, хотя allocation body спрятан в `.rld`.

## Два поля модели

После platform-specific base расположены только:

| Поле | PC | PS2 | Семантика |
|---|---:|---:|---|
| base mesh | `+0x58` | `+0x50` | intrusive relationship к базовому `spMesh` |
| projection group | `+0x5C` | `+0x54` | `u32` |

PS2 constructor `0x0015AA60` обнуляет base mesh и буквально записывает `3` в
projection group. Это исправляет прежнее предположение corpus-карточки:
отсутствующий serialized field нельзя нормализовать к эффективному нулю без
runtime-доказательства. На PS2 подтверждённый constructor default равен `3`;
PC constructor body защищён `.rld`, поэтому его default отдельно не заявлен.

Обе copy functions копируют intrusive base-mesh reference и projection group.
Setter заменяет reference с обычным decrement/delete/increment contract и
после этого вызывает runtime-mode recomputation. Clone создаёт новый объект,
регистрирует пару в clone manager и вызывает virtual copy.

## Связь с рендерингом

PC `0x00479DC0` и PS2 `0x0015A640` реализуют одинаковую трёхфазную схему:
вызывают inherited pre-render slot, передают `baseMesh` в renderer interface
slot `9`, затем вызывают inherited post-render slot. На PC передаётся поле
`+0x58`, на PS2 — `+0x50`; concrete renderer bodies находятся по
`0x004BC670/0x001FF6A0`. Это закрывает разрыв от scene traversal до
платформенного mesh backend без присвоения выдуманного original method name.

На PS2 renderer принимает не абстрактный data-объект напрямую: конкретный
runtime leaf [`spPS2Mesh`](native-class-sp-ps2-mesh.md) владеет подготовленным
`spPS2MeshData`, переносит из него primitive/vertex counts и передаёт packet
в platform helper. Тем самым путь модели уточнён до
`spModel -> spMesh/spRenderMesh -> spPS2Mesh -> renderer packet submission`.

Важная платформенная поправка: полный classifier `0..8` доказан только в PS2
body `0x00159F80`. Соответствующий vtable slot PC указывает на no-op
`0x0048EAA0`; PC debug/dump body `0x00479B00` занимает другой slot и не может
считаться classifier-ом. Пока material/pass types не реконструированы,
portable setter только инвалидирует runtime-mode cache; PS2 правила не
проецируются на PC.

Bounds slots делегируют данным base mesh. PC `0x00479D20/0x00479D40/
0x00479DA0` и PS2 `0x0015A770/0x0015A700/0x0015A6D0` возвращают sphere,
min/max и validity либо нулевые значения при пустой связи. Последующий разбор
`spModelSerializer` уточнил static contract: reader запрашивает class ID
`spMesh` (`0x3F077B6C`). Corpus-объект может быть concrete `spMeshData`, но
заужать само поле до этого leaf-класса было неверно.

## Проверяемая реконструкция и открытые вопросы

Добавлены RTTI/factory, clone/copy, renderable properties, base-mesh ownership,
projection group и раздельные PC/PS2 ABI structs/tests. Material/fog пока
типизированы широко (`spBaseObject`), чтобы не создавать фиктивные
`spMaterialData` и `spFog`; base mesh теперь корректно типизирован как
`spMesh`, а concrete `spMeshData` принимается полиморфно.

`python research/inspect_render_submission.py` фиксирует обе vtable, полные
тела model render и renderer submission, PC protected bridge/open D3D wrapper,
PS2 VIF/DMA/GS path и различие classifier slots: 37/37 проверок.

Остаются неизвестными source path, точный PC constructor/default group,
исходные имена draw/bounds slots, PS2 classifier rules как original enum и
семантика каждого projection group. Platform mesh/vertex resources уже
частично восстановлены; следующий обязательный разрыв находится в concrete
material/fog types и pre/post-render state.
