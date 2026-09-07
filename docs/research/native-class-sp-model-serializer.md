# `spModelSerializer`: mesh relationship и projection group

PC checkpoint10: [реальные sections и RenderNode→Model](native-pc-scene-serialization.md).
Factory exact14, reader4938F0, actual writer secondary4935F0→1404A30;
историческое body label493600 нельзя считать обычным callable writer.
Отсутствующий mesh допустим, explicitNULL идёт на493A04. Projection1 всегда
пишется с UInt32 length framing, не minimal fixed-size tag. Source adapter
реализован/сравнен, но nonempty mesh/material/fog, Skin, errors/lossless открыты.

Статус: source filename, identity, inheritance, storage-free layout, lifetime,
target, resource indexing и два собственных поля подтверждены на PC и PS2.
PC bounded stream секции теперь имеют actual native/source сравнения;
PS2 потоковая реализация этим не заявляется.

Оба executable сохраняют строку `spModelSerializer.cpp`, но не полный путь и
не header. Файлы реконструкции размещены в `Code/Sparkplug` по подтверждённой
модульной границе.

## Идентичность и ABI

`spModelSerializer` имеет Class ID `0xDB55C34A`. C++ constructor chain и engine
RTTI независимо дают direct base `spRenderableSerializer` (`0x4D694D82`).
Target slot возвращает `spModel::ClassID` (`0x763277DB`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x00762BC8` | `0x004AA7B0` |
| Initializer | `0x006D49B0` | `0x004836D0` |
| Factory | `0x004934C0` (protected entry) | `0x00196480` |
| Allocation | exact `0x14` (checkpoint10) | exact `0x14` |
| Primary vtable | `0x006ECBE4` | header `0x0048F8F0` |
| Secondary vtable | `0x006ECBD8` | header `0x0048F914` |

PS2 constructor `0x00196360` вызывает `spRenderableSerializer` constructor
`0x00197C70`, после чего меняет только два vptr. Собственного storage нет.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x00493480` | `0x00195E60` |
| write | `0x00493600` | `0x00195E70` |
| index resource graph | `0x004935A0` | `0x00196090` |
| read | `0x004938F0` | `0x00196100` |
| target class ID | `0x004934B0` | `0x001962E0` |
| deleting destructor | `0x00493580` | `0x001962F0` |
| blank clone | `0x00493530` | `0x001963A0` |

PC write thunk — `0x004935F0`. PS2 secondary thunks
`0x00196510/0x00196500/0x001964F0` адаптируют write, indexing и read
соответственно. Native write/read сначала вызывают соответствующий метод
`spRenderableSerializer`, затем открывают собственную model section.

## Собственные поля

| ID | Поле | Native write condition |
|---:|---|---|
| 0 | base mesh relationship | mesh не null |
| 1 | projection group | всегда |

Field 0 использует relationship target `spMesh::ClassID = 0x3F077B6C`.
Это уточняет предыдущий reconstruction type: объект в shipped corpus обычно
является concrete `spMeshData`, но контракт поля принимает базовый `spMesh`.
Portable `spModel` поэтому теперь хранит `shared_ptr<spMesh>`, сохраняя
совместимость с `spMeshData` без ложного заужения типа.

Field 1 записывается всегда, включая PS2 constructor default `3`. Index pass
сначала делегирует `spRenderableSerializer`, затем индексирует base mesh.
Reader принимает собственные поля в block-order; пустая обязательная model-to-
mesh связь на downstream path сопровождается native диагностикой
`No empty model->mesh relation allowed`, но точная стадия этой проверки пока не
переносится в portable writer plan.

## Portable срез и проверка

Восстановлены RTTI/factory, blank clone, target ID и раздельный
`KnownWritePlan`: inherited renderable section плюс model section. Для default
модели это `{renderable: AlphaSortEnable, AlphaSortPriority; model:
ProjectionGroup}`; непустой mesh добавляет `Base` перед group.

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты также
фиксируют, что relationship ожидает `spMesh`, а `spMeshData` остаётся допустимым
concrete объектом.

Block framing/common references реализованы. Полные rollback/status,
nonempty resource lifetime и остальные downstream consumers остаются открытыми.

## Открытые вопросы

1. Полный original source path/header и secondary-interface names.
2. Прямой PC allocation size и распакованные PC bodies.
3. Точная стадия/условие запрета пустой model-to-mesh связи.
4. Projection-group enum и platform-independent default.
5. Stream status, relationship table и rollback contract.
