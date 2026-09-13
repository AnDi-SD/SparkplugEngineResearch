# spMeshDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMeshDataSerializer](../../../Sparkplug/Code/Sparkplug/spMeshDataSerializer.h).

PC сохраняет точный translation-unit path:

`Z:\Sparkplug\Code\Sparkplug\spMeshDataSerializer.cpp`

PS2 независимо содержит filename `spMeshDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x66380037`, C++ и registered base — direct `spSerializer`, target —
`spMeshData::ClassID` (`0x33C34CF0`).

PS2 constructor `0x00162A90` вызывает `spSerializer` constructor и меняет
только оба vptr. Собственного storage нет.

## Методы

PS2 secondary thunks `0x00162C40/0x00162C30/0x00162C20` адаптируют write,
indexing и read. Indexing pass возвращает success без relationships: оба
буфера принадлежат самому `spMeshData`, а не object graph table.

## Stream contract

Собственная data-block секция содержит один field ID `0`. Diagnostic сохраняет
исходное написание `esfMeshDataCrossPatform` с опечаткой.

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

## Открытые вопросы

1. Original header и secondary-interface names.
2. Прямой PC allocation size и отдельный адрес cross-platform helper.
3. Имя/тип native selector и смысл всех его значений.
4. Buffer stream framing, status enum и rollback semantics.
5. Точный contract specialized load против обычного data-block read.
