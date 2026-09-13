# spPS2TextureDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2TextureDataSerializer](../../../Sparkplug/Code/Sparkplug/spPS2TextureDataSerializer.h).

PC сохраняет точный путь translation unit:

`Z:\Sparkplug\Code\Sparkplug\spPS2TextureDataSerializer.cpp`

## Идентичность и ABI

Class ID `0x43C76799`, C++ и registered base — прямой
`spTextureDataSerializer` (`0x1C4C75BA`), target — класс `0x24767C83`, которому
по строкам и месту в платформенной ветке соответствует `spPS2TextureData`.
Portable-класс target пока не объявлен: это имя и ID не превращены в выдуманную
layout-реализацию.

PS2 factory выделяет ровно `0x14` байт, вызывает constructor базового
`spTextureDataSerializer` `0x00178A80` и заменяет только два vptr. Derived-класс
не добавляет состояния. PC factory закрыт защитной секцией, поэтому размер там
помечен как наблюдаемый, а не как прямой `sizeof`.

## Методы

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

## Контракт reader

- при выключенной native-маске поле 0 загружается общим cross-platform helper,
  а поле 1 пропускается;
- при включённой native-маске поле 0 пропускается, а поле 1 загружается нативным
  helper;
- маска различается по ABI: PC — `0x02`, PS2 — `0x08`.

## payload поля 1

1. один raw byte; PS2 сохраняет его в `spTextureData+0x78`, без ветки пропуска
   последующих чтений при нуле (`00176AB4..00176AC4`);
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

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямой PC `sizeof` и распаковка protected factory entry.
3. Original enum/name native serialization mode и reader flags.
4. Исходные имена `auxiliaryValue` и четырёх слов mip descriptor.
5. Полная таблица pixel formats и правила swizzle/packing PS2.
6. Ownership/alignment payload, status enum и rollback при частичной ошибке.
7. Layout/lifetime самого `spPS2TextureData` за пределами уже доказанного target
   ID и унаследованного `spTextureData`-доступа.
