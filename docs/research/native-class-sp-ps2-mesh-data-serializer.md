# `spPS2MeshDataSerializer`: общий меш, PS2-пакет и границы

Статус: exact source identity, RTTI/lifetime, storage-free ABI, target, три
field ID, write order и развилка reader подтверждены на PC/PS2. Нативный
DMA/VIF/GIF codec остаётся evidence-only.

PC сохраняет точный translation-unit path:

`Z:\Sparkplug\Code\Sparkplug\spPS2MeshDataSerializer.cpp`

PS2 независимо содержит filename `spPS2MeshDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x6B0C238F`, C++ и registered base — direct
`spMeshDataSerializer` (`0x66380037`), target — `spPS2MeshData::ClassID`
(`0x737D740F`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E398` | `0x004A9250` |
| Initializer | `0x006D2BA0` | `0x00482820` |
| Factory | `0x0042A2A0` (protected entry) | `0x00163890` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006DD2DC` | header `0x0048E490` |
| Secondary vtable | `0x006DD2D0` | header `0x0048E4B4` |

PS2 factory вызывает constructor `spMeshDataSerializer` `0x00162A90`, затем
меняет только два vptr. Clone делает то же самое и выделяет ровно `0x14` байт.
На PC соседняя с таблицей строка относится к другой registration-записи;
принадлежность vtable PS2-сериализатору проверена по записям vptr в destructor
и по вызовам writer/reader, а не по положению строки в `.rdata`.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0042A230` | `0x00162C50` |
| generic `SBOO` load slot | `0x0042AFD0` | `0x00162990` |
| load platform-specific payload | `0x0042A420` | `0x00163160` |
| serialize platform-specific payload | `0x0042A600` | `0x00162C70` |
| write | `0x0042A700` | `0x00162E10` |
| index resource graph | `0x005A7DB0` | `0x00162C60` |
| read | `0x0042AB40` | `0x00163350` |
| target class ID | `0x0042A260` | `0x00163730` |
| deleting destructor | `0x0042A360` | `0x00163740` |
| blank clone | `0x0042A310` | `0x001637B0` |

PS2 secondary thunks `0x00163920/0x00163910/0x00163900` адаптируют write,
indexing и read. Indexing возвращает success без relationships: payload
содержит данные, а не ссылки resource graph.

## Контракт writer

Собственная data-block секция использует три field ID типа `7`:

| ID | Диагностическое имя | Условие |
|---:|---|---|
| 0 | `esfMeshDataCrossPatform` | native mode равен `0` или `2` |
| 1 | `esfMeshDataPlatformSpecific` | всегда |
| 2 | `esfMeshDataBoundingBox` | всегда |

Опечатка `CrossPatform` сохранена в обеих сборках. Порядок строго `0? -> 1 ->
2`. Поле 0 вызывает уже доказанный helper базового сериализатора и пишет
собственные index buffer, затем vertex buffer. Поле 1 строит временный
`spPS2MeshData` из тех же двух буферов входного `spMeshData` и пишет его
нативное представление. Поэтому target reader-а `spPS2MeshData` не означает,
что writer получает готовый PS2 packet: его входом служит общий mesh data.

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

Семантика флага общая, но маска в layout конфигурации различается: PC проверяет
`0x02`, PS2 — `0x08`. Эти значения сохранены раздельно в ABI и portable API;
объединять их в выдуманный общий enum нельзя.

## Portable-срез

Восстановлены RTTI/factory, blank clone, target ID, безопасный план полей и две
раздельные проверки native-load mask. Реальный packet builder/loader не
перенесён: он зависит от ещё не названных descriptor fields, PS2 DMA/VIF/GIF
правил, allocator alignment и общего serializer status/rollback API.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты
фиксируют direct base, exact PS2/observed PC `0x14`, target, write plans
`0|2 -> {CrossPlatform, PlatformSpecific, BoundingBox}` и
`other -> {PlatformSpecific, BoundingBox}`, разные masks `0x02/0x08` и blank
clone.

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямое PC `sizeof` и распаковка protected factory entry.
3. Имя/тип native serialization mode и имя reader configuration flags.
4. Названия всех PS2 descriptor fields и точная packet grammar.
5. Ownership/alignment временного PS2 packet, status enum и rollback.
