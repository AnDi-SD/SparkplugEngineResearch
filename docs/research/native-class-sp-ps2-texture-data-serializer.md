# `spPS2TextureDataSerializer`: PS2-представление текстуры и выбор payload

Статус: точный source identity на PC, RTTI/lifetime, storage-free ABI, target,
field IDs, порядок writer, развилка reader и бинарный каркас нативного payload
подтверждены на PC и PS2. Имена части полей изображения и настоящий stream codec
пока остаются evidence-only.

PC сохраняет точный путь translation unit:

`Z:\Sparkplug\Code\Sparkplug\spPS2TextureDataSerializer.cpp`

PS2 независимо сохраняет имя класса `spPS2TextureDataSerializer`, но отдельной
строки с именем `.cpp` в проверенном ELF не найдено.

## Идентичность и ABI

Class ID `0x43C76799`, C++ и registered base — прямой
`spTextureDataSerializer` (`0x1C4C75BA`), target — класс `0x24767C83`, которому
по строкам и месту в платформенной ветке соответствует `spPS2TextureData`.
Portable-класс target пока не объявлен: это имя и ID не превращены в выдуманную
layout-реализацию.

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E4C8` | `0x004A9B50` |
| Initializer | `0x006D2C30` | `0x00482E90` |
| Factory | `0x0042C9E0` (protected entry) | `0x001771E0` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006DDBA8` | header `0x0048EC90` |
| Secondary vtable | `0x006DDB9C` | header `0x0048ECB4` |

PS2 factory выделяет ровно `0x14` байт, вызывает constructor базового
`spTextureDataSerializer` `0x00178A80` и заменяет только два vptr. Derived-класс
не добавляет состояния. PC factory закрыт защитной секцией, поэтому размер там
помечен как наблюдаемый, а не как прямой `sizeof`.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0042C900` | `0x00176320` |
| serialize native payload | `0x0042CD90` | `0x00176330` |
| write | `0x0042D1B0` | `0x001765D0` |
| index resource graph | `0x005A7DB0` | `0x001765C0` |
| load native payload | `0x0042D6C0` | `0x00176A10` |
| read | `0x0042D910` | `0x00176CF0` |
| target class ID | `0x0042C930` | `0x00177080` |
| destructor | `0x0042C910` | `0x00177090` |
| deleting destructor | `0x0042CAA0` | ABI совмещён с destructor |
| blank clone | `0x0042CA50` | `0x00177100` |

PS2 secondary thunks `0x00177820/0x00177810/0x00177800` корректируют
`this-0x10` для write/index/read. Indexing возвращает success без отношений:
локальный payload содержит байты, а не ссылки resource graph.

## Контракт writer

Сначала вызывается доказанный source-wrapper базового сериализатора. Embedded
memory stream завершает запись полем 3, обычный stream — reference-полем 4.
При отсутствии внешнего source поле 2 открывает локальное тело со строгим
порядком:

1. поле 6 `PlatformType`;
2. поле 0 `CrossPlatform`, только для native mode `0` или `2`;
3. поле 1 `PlatformSpecific`, всегда.

Значение `PlatformType` равно `9`, если mode равен `0` или `2`, и `8` во всех
остальных наблюдаемых режимах. На PC отдельный helper `0x00429760` явно
реализует условие `mode == 0 || mode == 2`; PS2 writer проверяет те же значения
непосредственно.

## Контракт reader

Reader сначала разрешает embedded/reference source общим helper. Для локального
тела он dispatch-ит поля `0`, `1` и `6`, а неизвестные ID пропускает общей
веткой. Поле 6 читается как 32-битное значение, но проверенного решения на его
основе в этом методе нет.

- при выключенной native-маске поле 0 загружается общим cross-platform helper,
  а поле 1 пропускается;
- при включённой native-маске поле 0 пропускается, а поле 1 загружается нативным
  helper;
- маска различается по ABI: PC — `0x02`, PS2 — `0x08`.

Это выбор одного payload, а не последовательная загрузка обоих.

## Нативный payload поля 1

Обе сборки подтверждают одинаковый порядок данных:

1. один byte-флаг наличия platform-specific data;
2. `uint32 pixelFormat`;
3. `uint32 width`;
4. `uint32 height`;
5. `uint32 auxiliaryValue` — исходное имя и точная семантика неизвестны;
6. `uint32 mipCount`;
7. необязательная palette: `0x40` байт при format `0`, `0x400` при format `1`,
   для прочих наблюдаемых значений отсутствует;
8. `mipCount` записей: четыре `uint32`, затем raw data размером в четвёртое
   слово записи.

In-memory mip record занимает `0x14`: четыре слова заголовка и pointer на data.
PC хранит контейнер как begin/end и вычисляет count делением разницы на `0x14`;
PS2 хранит count и storage pointer раздельно. Это платформенное различие
намеренно не сведено в общий native layout.

Названия четырёх mip-слов, значение `auxiliaryValue`, допустимые pixel formats,
ограничения размеров и ownership выделенной памяти пока не доказаны. Portable
слой поэтому описывает только подтверждённый префикс, размеры palette и порядок
полей, но не читает и не пишет настоящий SMO-stream.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты фиксируют
direct base, target, source termination, планы `2 -> 6 -> 0? -> 1`, platform
type `8/9`, раздельные masks `0x02/0x08`, размеры palette, native header и blank
clone.

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямой PC `sizeof` и распаковка protected factory entry.
3. Original enum/name native serialization mode и reader flags.
4. Исходные имена `auxiliaryValue` и четырёх слов mip descriptor.
5. Полная таблица pixel formats и правила swizzle/packing PS2.
6. Ownership/alignment payload, status enum и rollback при частичной ошибке.
7. Layout/lifetime самого `spPS2TextureData` за пределами уже доказанного target
   ID и унаследованного `spTextureData`-доступа.
