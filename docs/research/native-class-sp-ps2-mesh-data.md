# Нативный класс `spPS2MeshData`

Дата разбора: 2026-09-04. Статус: identity, hierarchy, PS2 exact layout,
совпадающий PC observed extent, constructor tables, packet ownership,
нормализация vertex layout и связь с serializer подтверждены. Низкоуровневая
генерация исполняемой PS2 DMA/VIF/GIF-цепочки сохранена как evidence, но
намеренно не эмулируется переносимым классом.

| Факт | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x737D740F / spPlatformSpecificMeshData` | same |
| Registration / initializer | `0x007608D8 / 0x006D3CA0` | `0x004A9130 / 0x00482760` |
| Factory | `0x00473FA0` (`.rld` thunk) | `0x001611A0` |
| Constructor | protected body | `0x00160FA0` |
| Destructor | `0x00474050`, deleting `0x00474A60` | `0x00160F00` |
| RTTI clone | `0x00474000` | `0x001610E0` |
| Registration getter | `0x00473160` | `0x0015D550` |
| Vtable | `0x006E9454` | header `0x0048E3A0` |
| Размер | observed extent `0x100` | exact allocation `0x100` |

Original path самого data-класса строками не сохранился, поэтому
`Code/Sparkplug/spPS2MeshData.*` остаётся inferred. Связанный serializer имеет
точный PC source path
`Z:\Sparkplug\Code\Sparkplug\spPS2MeshDataSerializer.cpp`. Эта строка
подтверждает модульную границу, но не доказывает имя translation unit самого
data-класса.

## Layout и время жизни

PS2 factory непосредственно запрашивает `0x100` байт. PC factory скрыт
за защищённой `.rld`-веткой, однако virtual methods, destructor и serializer
обращаются ко всем полям вплоть до `+0xFC`; поэтому на PC честно зафиксирован
полный observed extent `0x100`, но не exact `sizeof`.

| Offset | Роль | Evidence |
|---:|---|---|
| `+0x00..+0x13` | `spPlatformSpecificMeshData` | общий RTTI base и exact base layout `0x14` |
| `+0x14` | тип/режим подготовленного index stream | default `4`; type `2 -> 3`, type `3 -> 4` |
| `+0x18` | selector vertex-комбинации | helper `0x0015D650`; PC-аналог `0x00473E80` |
| `+0x1C` | effective component flags | пишется после нормализации vertex buffer |
| `+0x20` | число упаковываемых streams | helper `0x0015D570`; PC-аналог `0x00473ED0` |
| `+0x24` | transient/non-owning `spVertexBuffer*` | используется только при построении packet |
| `+0x28` | transient/non-owning `spIndexBuffer*` | то же |
| `+0x2C` | source index count | заполняется preparation path из входного index buffer |
| `+0x30` | emitted vertex count | builder вычисляет число выходных вершин; `spPS2Mesh` копирует в `spMesh +0x4C` |
| `+0x34` | primitive count | берётся из `spIndexBuffer +0x18`; `spPS2Mesh` копирует в `spMesh +0x48` |
| `+0x38..+0x3C` | component-group counters | считаются из effective component flags; original names неизвестны |
| `+0x40` | DMA/VIF/GIF packet | aligned allocation, owned при `+0xFC == 0` |
| `+0x44` | размер packet в qwords | serializer читает и пишет вместе с payload |
| `+0x48` | 22 descriptor words | точные constructor defaults восстановлены |
| `+0xA0` | 22 signed order values | точные constructor defaults восстановлены |
| `+0xF8` | grouping/order option | default `1`, читается packet builder |
| `+0xFC` | external/non-owning packet flag | default `0`; управляет освобождением `+0x40` |

PS2 constructor записывает `+0x14 = 4`, очищает рабочие поля, заполняет обе
22-элементные таблицы, устанавливает `+0xF8 = 1` и `+0xFC = 0`.
Descriptor table `+0x48` содержит:

- index `0 = 0x1006C`;
- index `1 = -1`;
- indexes `2..6 = 0x1006C`;
- index `7 = 0x1006F`, `8 = -1`, `9 = 0x6E`, `10..11 = -1`;
- indexes `12..19 = 0x10064`, `20..21 = -1`.

Order table `+0xA0` содержит `12..19` в indexes `0..7`, затем `9, 7, 0` и
одиннадцать `-1`. Portable constructor хранит именно эти значения, поскольку
это платформенно нейтральное наблюдаемое состояние и полезная проверка layout.

Destructor сначала синхронизирует renderer через `0x00411328`. Если packet
не external, он освобождает исходную aligned allocation по адресу
`packet - 0x10`; затем обнуляет `+0x40` и вызывает destructor базы. Поле
`+0xFC` тем самым является lifetime-флагом, а не признаком наличия данных.

RTTI clone на обеих платформах создаёт свежий объект и вызывает только
inherited name-copy slot. Packet, transient inputs и рабочие счётчики не
копируются. Это намеренно отдельно от serializer/conversion path.

## Подготовка из общих buffers

Основная функция `0x00160D10` на PS2 соответствует PC `0x00474FF0` и принимает
`spIndexBuffer` плюс `spVertexBuffer`:

1. null buffers и index types вне `2/3/4` отклоняются;
2. type `4` считается уже подготовленным и немедленно возвращает успех;
3. для type `2/3` требуется component bit `0x800` и один из bits
   `0x40/0x100`;
4. если комбинации нет, `0x00160900` создаёт временную расширенную копию
   vertex buffer: добавляет `0x800` и, при отсутствии обоих альтернативных
   bits, `0x100`;
5. рассчитываются selector `+0x18`, effective flags `+0x1C`, stream count
   `+0x20`, source index count `+0x2C`, counts bits `12..18` в `+0x38` и bits
   `1..4` в `+0x3C`;
6. `0x0015F900` строит packet в `+0x40/+0x44` и временный vertex buffer
   освобождается.

Сопоставление type в `+0x14` равно `2 -> 3` и `3 -> 4`. Наблюдённые helpers
выбора компонента и подсчёта streams реализованы в
`PreparationPlan`; этот объект не меняет mesh data и позволяет проверять
решение native-функции безопасно.

Собственно packet builder формирует EE/GS-специфичную цепочку DMA, VIF unpack и
GIF records. Virtual paths `0x0015F1F0` и `0x0015F030` формируют 16-байтные
records для разных способов группировки. Их PC-аналоги — `0x00475880` и
`0x00474E00`. Восстанавливать эти байты как host-renderer API нельзя: без
контракта PS2 DMA memory, cache synchronization и GS state такая реализация
выглядела бы правдоподобно, но была бы ложной.

## Связь с serializer

`spPS2MeshDataSerializer` имеет общий class ID `0x6B0C238F`, base ID
`0x66380037` и exact source-path evidence, приведённое выше.

| Факт | PC | PS2 |
|---|---:|---:|
| Registration / initializer | `0x0075E398 / 0x006D2BA0` | `0x004A9250 / 0x00482820` |
| Factory | `0x0042A2A0` | `0x00163890` |
| Размер serializer | не доказан напрямую | exact `0x14`, storage-free subclass |
| Load | `0x0042A420` | связь подтверждена registration/consumer paths |
| Convert from `spMeshData` | `0x0042A600` | общий platform pipeline |

PC conversion `0x0042A600` берёт owning buffers `spMeshData +0x50/+0x54`,
создаёт `spPS2MeshData` через его factory и вызывает `0x00474FF0`. PC loader
читает packet metadata и payload, заполняет `+0x18/+0x1C/+0x20/+0x30..+0x44`
и присоединяет полученный platform object к `spMesh`, после чего пересчитывает
bounds. Это доказывает, что `spPS2MeshData` — реальный cross-platform resource
type даже в PC executable, а не случайно оставшийся PS2 symbol.

## Реализованный срез и открытое

Portable-класс сохраняет native RTTI hierarchy, concrete factory, name-only
clone, exact constructor tables и аппаратно-независимое планирование
нормализации. ABI layouts остаются отдельными: PS2 exact `0x100`, PC observed
`0x100`. Исполняемый packet не создаётся и состояние его наличия остаётся
безопасно пустым.

Открыты:

- прямой PC `sizeof` и constructor body за `.rld`;
- original header/TU и настоящие имена всех fields/methods/enums;
- original имена счётчиков `+0x30/+0x34` и все варианты влияния `+0xF8`;
- полная бинарная grammar serializer и external packet load mode;
- точное сопоставление descriptor words с GS/VIF component formats;
- безопасная архитектура будущего PS2 backend, если проект когда-либо будет
  исполнять, а не только разбирать эти packets.
