# `spMeshDataSerializer`: cross-platform geometry payload

Статус: exact source identity, RTTI/lifetime, storage-free ABI, target,
platform/mode gate и index/vertex buffer order подтверждены на PC/PS2.
Byte stream codec оставлен evidence-only.

PC сохраняет точный translation-unit path:

`Z:\Sparkplug\Code\Sparkplug\spMeshDataSerializer.cpp`

PS2 независимо содержит filename `spMeshDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x66380037`, C++ и registered base — direct `spSerializer`, target —
`spMeshData::ClassID` (`0x33C34CF0`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E400` | `0x004A91F0` |
| Initializer | `0x006D2BD0` | `0x004827E0` |
| Factory | `0x0042AEF0` (protected entry) | `0x00162BB0` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006DD4B4` | header `0x0048E430` |
| Secondary vtable | `0x006DD4A8` | header `0x0048E454` |

PS2 constructor `0x00162A90` вызывает `spSerializer` constructor и меняет
только оба vptr. Собственного storage нет.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0042AEB0` | `0x001622D0` |
| specialised load | `0x0042AFD0` | `0x001622E0` |
| cross-platform helper | protected/merged | `0x00162470` |
| write | `0x0042B170` | `0x00162510` |
| index resource graph | `0x005A7DB0` | `0x00162500` |
| read | `0x0042B420` | `0x00162730` |
| target class ID | `0x0042AEE0` | `0x00162A10` |
| deleting destructor | `0x0042AFB0` | `0x00162A20` |
| blank clone | `0x0042AF60` | `0x00162AD0` |

PS2 secondary thunks `0x00162C40/0x00162C30/0x00162C20` адаптируют write,
indexing и read. Indexing pass возвращает success без relationships: оба
буфера принадлежат самому `spMeshData`, а не object graph table.

## Stream contract

Собственная data-block секция содержит один field ID `0`. Diagnostic сохраняет
исходное написание `esfMeshDataCrossPatform` с опечаткой.

Writer проверяет пока не названный native selector. Значения `0` и `2`
записывают field 0; остальные значения пропускают секцию. Внутри field payload
порядок строгий:

1. `spIndexBuffer::Serialize(stream)` для `+0x50`;
2. `spVertexBuffer::Serialize(stream)` для `+0x54`.

Ни один буфер не является relationship. Writer не подавляет field по null/default
состоянию: valid runtime object обязан предоставить оба буфера. Reader для
field 0 создаёт оба concrete buffer, последовательно читает их и передаёт в
virtual mesh-data initializer. При ошибке временные объекты освобождаются.
Неизвестные field IDs идут через общий skip path.

Specialised load `0x001622E0` читает ту же пару буферов без внешнего field
framing и затем выполняет тот же initializer/cleanup. Отдельный helper
`0x00162470` пишет эту пару напрямую; platform-specific serializers используют
его как вложенный payload.

## Portable срез

Восстановлены RTTI/factory, blank clone, target ID и безопасный план field 0.
Поскольку original type и смысл selector ещё не доказаны, API принимает
`nativeSerializationMode` как `uint32_t` и не придумывает enum: только значения
`0/2` имеют подтверждённое поведение.

Реальный stream codec не дублируется поверх уже восстановленных аналитических
buffer facades: общий dispatch `spSerializerManager` теперь восстановлен
отдельно, но framing/status types, точная привязка этого codec к manager/FAT и
rollback всё ещё не закрыты.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты
фиксируют direct base, exact PS2/observed PC `0x14`, target, gate
`0|2 -> {CrossPlatform}` / остальные значения `-> {}` и blank clone.

## Открытые вопросы

1. Original header и secondary-interface names.
2. Прямой PC allocation size и отдельный адрес cross-platform helper.
3. Имя/тип native selector и смысл всех его значений.
4. Buffer stream framing, status enum и rollback semantics.
5. Точный contract specialized load против обычного data-block read.
