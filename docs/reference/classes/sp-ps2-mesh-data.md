# spPS2MeshData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2MeshData](../../../Sparkplug/Code/Sparkplug/spPS2MeshData.h).

| Факт | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x737D740F / spPlatformSpecificMeshData` | same |
| RTTI clone | `0x00474000` | `0x001610E0` |
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

| Offset | Роль |
| ---: | --- |
| `+0x00..+0x13` | `spPlatformSpecificMeshData` |
| `+0x14` | тип/режим подготовленного index stream |
| `+0x18` | selector vertex-комбинации |
| `+0x1C` | effective component flags |
| `+0x20` | число упаковываемых streams |
| `+0x24` | transient/non-owning `spVertexBuffer*` |
| `+0x28` | transient/non-owning `spIndexBuffer*` |
| `+0x2C` | source index count |
| `+0x30` | emitted vertex count |
| `+0x34` | primitive count |
| `+0x38..+0x3C` | component-group counters |
| `+0x40` | DMA/VIF/GIF packet |
| `+0x44` | размер packet в qwords |
| `+0x48` | 22 descriptor words |
| `+0xA0` | 22 signed order values |
| `+0xF8` | grouping/order option |
| `+0xFC` | external/non-owning packet flag |

PS2 constructor записывает `+0x14 = 4`, очищает рабочие поля, заполняет обе
22-элементные таблицы, устанавливает `+0xF8 = 1` и `+0xFC = 0`.
Descriptor table `+0x48` содержит:

- index `0 = 0x1006C`;
- index `1 = -1`;
- indexes `2..6 = 0x1006C`;
- index `7 = 0x1006F`, `8 = -1`, `9 = 0x6E`, `10..11 = -1`;
- indexes `12..19 = 0x10064`, `20..21 = -1`.

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

Packet builder формирует EE DMA/VIF chain и данные для последующей VU/GS
работы. Virtual paths `0x0015F1F0` и `0x0015F030` обслуживают различные index
paths; их PC-аналоги — `0x00475880` и `0x00474E00`. Для Type2 восстановлена
ограниченная сборка всех окончательных bytes, с отдельными streams либо
interleaved records. Это аналитический byte buffer: он не отправляется
host renderer, DMA или GS. Порядок normalization writes, component packing,
zero padding, chunk capacity и незакрытые ветви описаны в
[PS2 mesh preparation](../../engine/rendering/ps2-mesh-preparation.md).

## Связь с serializer

| Факт | PC | PS2 |
| --- | ---: | ---: |
| Размер serializer | не доказан напрямую | exact `0x14`, storage-free subclass |
| Load | `0x0042A420` | связь подтверждена registration/consumer paths |
| Convert from `spMeshData` | `0x0042A600` | общий platform pipeline |

PC conversion `0x0042A600` берёт owning buffers `spMeshData +0x50/+0x54`,
создаёт `spPS2MeshData` через его factory и вызывает `0x00474FF0`. PC loader
читает packet metadata и payload, заполняет `+0x18/+0x1C/+0x20/+0x30..+0x44`
и присоединяет полученный platform object к `spMesh`, после чего пересчитывает
bounds. Это доказывает, что `spPS2MeshData` — реальный cross-platform resource
type даже в PC executable, а не случайно оставшийся PS2 symbol.

## Границы описания

Portable-класс сохраняет native RTTI hierarchy, concrete factory, name-only
clone, exact constructor tables и планирование. Дополнительно реализованы
raw-copy/default writes нормализации с маской untouched bytes и целый
окончательный packet для поддержанных Type2 constructor formats, включая
compatible weighted layouts с1..4 weights и bone indices. Grouped compressed
Normal возвращает явный incomplete result. Expanded weighted normalization
требует явных captured allocation bytes, иначе тоже incomplete. Type3 возвращает известный native false: PS2 stripifier `0x001C4FE0`
безусловно отклоняет операцию. Для interleaved доступны raw Normal words.

Отдельный prepopulated-strip API записывает payload через общий Type2
component emitter с явными indices/ADC. Он не вызывает Type3 stripifier и
не меняет outer packet state. Bounded unweighted UV0/Color payload в обоих
grouping режимах сравнен с целым original direct return и временным frame
cleanup; terminal allocator остаётся отдельной моделью.

Integer reservation `0x00160750/0x001604B0` вынесен в отдельный
[captured-value helper](../../../Sparkplug/Analysis/PS2/spPS2MeshStripSizing.h).
Он сохраняет low32 результат и различие capacity A2, declared list count и
counts реально пройденных records. Это исходник size операции, а не
доказательство успешной внешней Type3 подготовки или безопасного allocation
extent для произвольного captured state.

ABI layouts остаются отдельными: PS2 exact `0x100`, PC observed `0x100`.
Owning host vector и metadata — аналитическое представление, не PS2 DMA
memory. Признак prepared появляется после успешной сборки этих bytes;
никакого исполнения или native allocator parity он не утверждает.

Открыты:

- прямой PC `sizeof` и constructor body за `.rld`;
- original header/TU и настоящие имена всех fields/methods/enums;
- original имена счётчиков `+0x30/+0x34` и все варианты влияния `+0xF8`;
- другие direct/prepopulated layouts, native allocator/fault domains и интеграция с внешним builder; bounded direct payload/end/frame-free и отдельный reservation helper не закрывают положительный Type3 path;
- unknown allocation bytes без переданного snapshot и grouped compressed Normal;
- нестандартные descriptor/order tables и native fault/allocation side effects;
- полная бинарная grammar serializer и external packet load mode;
- точное сопоставление descriptor words с GS/VIF component formats;
- безопасная архитектура будущего PS2 backend, если проект когда-либо будет
  исполнять, а не только разбирать эти packets.
