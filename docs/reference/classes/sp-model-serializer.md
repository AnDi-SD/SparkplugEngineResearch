# spModelSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spModelSerializer](../../../Sparkplug/Code/Sparkplug/spModelSerializer.h).

Статус: source filename, identity, inheritance, storage-free layout, lifetime,
target, resource indexing и два собственных поля подтверждены на PC и PS2.
PC bounded stream секции теперь имеют actual native/source сравнения;
PS2 потоковая реализация этим не заявляется.

Файлы реконструкции размещены в `Code/Sparkplug` по подтверждённой модульной границе.

## Идентичность и ABI

`spModelSerializer` имеет Class ID `0xDB55C34A`. C++ constructor chain и engine
RTTI независимо дают direct base `spRenderableSerializer` (`0x4D694D82`).
Target slot возвращает `spModel::ClassID` (`0x763277DB`).

PS2 constructor `0x00196360` вызывает `spRenderableSerializer` constructor
`0x00197C70`, после чего меняет только два vptr. Собственного storage нет.

## Методы

PC write thunk — `0x004935F0`. PS2 secondary thunks
`0x00196510/0x00196500/0x001964F0` адаптируют write, indexing и read
соответственно. Native write/read сначала вызывают соответствующий метод
`spRenderableSerializer`, затем открывают собственную model section.

## Собственные поля

| ID | Поле | Native write condition |
| ---: | --- | --- |
| 0 | base mesh relationship | mesh не null |
| 1 | projection group | всегда |

Field 0 использует relationship target `spMesh::ClassID = 0x3F077B6C`.
Это уточняет предыдущий reconstruction type: объект в известных данных обычно
является concrete `spMeshData`, но контракт поля принимает базовый `spMesh`.
Portable `spModel` поэтому теперь хранит `shared_ptr<spMesh>`, сохраняя
совместимость с `spMeshData` без ложного заужения типа.

Восстановлены RTTI/factory, blank clone, target ID и раздельный
`KnownWritePlan`: inherited renderable section плюс model section. Для default
модели это `{renderable: AlphaSortEnable, AlphaSortPriority; model:
ProjectionGroup}`; непустой mesh добавляет `Base` перед group.

Block framing/common references реализованы. Полные rollback/status,
nonempty resource lifetime и остальные downstream consumers остаются открытыми.

## Открытые вопросы

1. Полный original source path/header и secondary-interface names.
2. Прямой PC allocation size и распакованные PC bodies.
3. Точная стадия/условие запрета пустой model-to-mesh связи.
4. Projection-group enum и platform-independent default.
5. Stream status, relationship table и rollback contract.
