# spPS2MeshDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2MeshDataSerializer](../../../Sparkplug/Code/Sparkplug/spPS2MeshDataSerializer.h).

PC сохраняет точный translation-unit path:

`Z:\Sparkplug\Code\Sparkplug\spPS2MeshDataSerializer.cpp`

PS2 независимо содержит filename `spPS2MeshDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x6B0C238F`, C++ и registered base — direct
`spMeshDataSerializer` (`0x66380037`), target — `spPS2MeshData::ClassID`
(`0x737D740F`).

## Методы

PS2 secondary thunks `0x00163920/0x00163910/0x00163900` адаптируют write,
indexing и read. Indexing возвращает success без relationships: payload
содержит данные, а не ссылки resource graph.

## Контракт writer

Собственная data-block секция использует три field ID типа `7`:

| ID | Диагностическое имя | Условие |
| ---: | --- | --- |
| 0 | `esfMeshDataCrossPatform` | native mode равен `0` или `2` |
| 1 | `esfMeshDataPlatformSpecific` | всегда |
| 2 | `esfMeshDataBoundingBox` | всегда |

Подтверждённая часть platform-specific payload одинакова по смыслу на PC/PS2:

1. 16 байт descriptor header начиная с native offset `+0x18`;
2. шесть 32-битных значений из `+0x34`, `+0x30`, `+0x1C`, `+0x44`, `+0x38`,
   `+0x3C`;
3. `field44 * 16` байт packet data из `+0x40`.

Поле 2 пишет два непрерывных вектора по 12 байт: minimum из mesh offsets
`+0x2C..+0x34`, затем maximum из `+0x38..+0x40`.

## Контракт reader и различие платформ

Reader dispatch-ит `0/1/2`, неизвестные ID пропускает общим skip path и затем
финализирует объект.

- field 0 при cross-platform конфигурации создаёт `spIndexBuffer` и
  `spVertexBuffer`, читает их и вызывает initializer выходного PS2 mesh;
- field 1 при native конфигурации читает сохранённый packet напрямую;
- field 2 всегда восстанавливает minimum/maximum;
- payload, не выбранный текущей конфигурацией, пропускается, а не читается
  обоими путями сразу.

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямое PC `sizeof` и распаковка protected factory entry.
3. Имя/тип native serialization mode и имя reader configuration flags.
4. Названия всех PS2 descriptor fields и точная packet grammar.
5. Ownership/alignment временного PS2 packet, status enum и rollback.
