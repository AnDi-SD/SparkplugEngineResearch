# `spTextureData` (`0x78EA082B`)

Статус: контейнеры источника, платформы, палитры и mip-уровней полностью
восстановлены для чтения на всём доступном PC/PS2-корпусе. Viewer показывает
структурные поля и продолжает выводить BGRA-превью PC/cross-platform вариантов.
Нативные PS2-буферы разобраны без остатка по границам, но их swizzle и точное
преобразование каналов в BGRA пока намеренно не угадываются.
[CP125](tool-texture-writer-2026-09-08.md) добавил запись/resize одного встроенного
PC BGRA mip; [CP130](tool-importer-textures-2026-09-08.md) подключил тот же
структурный writer к новым ресурсам Importer. PS2/multiple-mip запись не включена.

Нативный layout/lifetime и concrete buffer-copy path теперь вынесены в
[`native-class-sp-texture-data.md`](native-class-sp-texture-data.md). Эта
карточка по-прежнему описывает именно сериализованное SMO-представление.

## Распространённость

| Корпус | Уникальные объекты | SMO | Физические вхождения объектов | Размер объекта |
|---|---:|---:|---:|---:|
| `pc-working` | 2 564 | 373 | 2 564 | 320–1 048 640 |
| `pc-pristine` | 2 564 | 373 | 2 564 | 320–1 048 640 |
| `ps2-pristine` | 2 357 | 294 | 6 439 | 171–263 243 |

Все 7 485 уникальных корпусных записей имеют имя и строго декодируются. Working
и pristine PC совпадают по структуре и содержимому текстур во всех 373 ресурсах.
PC и PS2 имеют 294 общих canonical resource path, но платформенные
представления ожидаемо различаются.

## Прямые поля объекта

Executable обеих платформ подтверждают имена:

| Field | Имя serializer | Наблюдение в прямой секции |
|---:|---|---|
| 0 | `esfTextureDataCrossPlatform` | 96 объектов в каждом PC-корпусе |
| 1 | `esfTextureDataPlatformSpecific` | используется внутри embedded payload |
| 2 | `esfTextureDataSourceNone` | подтверждено executable, прямо не наблюдается |
| 3 | `esfTextureDataSourceEmbeded` | 2 468 объектов в каждом PC-корпусе, все 2 357 PS2 |
| 4 | `esfTextureDataSourceReference` | подтверждено executable, прямо не наблюдается |
| 6 | `esfTextureDataPlatformType` | семь legacy PC-объектов перед прямым field 0 |

В написании строки executable действительно используется `Embeded` с одной
буквой `d`. Поля 0/1/6 повторяются во вложенной производной секции; одинаковый
номер на разных уровнях не означает один и тот же физический offset.

## Пять наблюдаемых storage-вариантов

| Вариант БД | PC working | PC pristine | PS2 pristine | Смысл |
|---|---:|---:|---:|---|
| `texture_legacy_cross` | 96 | 96 | 0 | прямой field 0 с BGRA32 |
| `texture_direct3d_embedded` | 2 433 | 2 433 | 0 | embedded Direct3D BGRA32, 1–9 mip |
| `texture_ps2_native` | 6 | 6 | 2 353 | embedded PS2 без cross-копии |
| `texture_ps2_native_with_cross` | 28 | 28 | 4 | одновременно BGRA и PS2 native |
| `texture_embedded_cross_only` | 1 | 1 | 0 | embedded field 0 без native-копии |

Таким образом, PC-корпус сам содержит 34 настоящих PS2-native текстуры — прежде
всего в menu-файлах `*_ps2`. Платформу нельзя определять только по каталогу или
маске FFPS; внутренний layout распознаётся по собственной структуре.

## Cross-platform BGRA32

Прямой либо вложенный field 0 содержит один field 5 и terminator:

```text
field 5 payload:
    UInt32 width
    UInt32 height
    UInt32 pixelFormat          // наблюдаемые 0/1 дают четырёхбайтовые pixels
    UInt32 bytesPerPixel        // всегда 4
    byte bgra[width*height*4]
terminator
```

Размер проверяется точным равенством `16 + width * height * 4`. Это обычный
несжатый BGRA32; Viewer может показывать его без платформенной конверсии.

Первоначальное имя `auxiliaryValue` третьего слова устарело:
[оригинальный cross reader CP115–116](native-pc-texture-cross-upload.md)
подтвердил pixel format отдельно от следующего pixel size. В текущем C# record
оно находится в `FormatValue`; `AuxiliaryValue` cross-представления исторически
содержит bytes-per-pixel. Переносить последнее в format запрещено.
Bare legacy field 0 и корректная embedded-обёртка имеют разное поведение
оригинального DX reader; успешный структурный decode сам по себе не доказывает
инициализацию runtime-текстуры.

## Embedded-обёртка

Field 3 всегда содержит две serializer-секции:

```text
section 0, source base:
    field 2, byte false
    terminator

section 1, texture implementation:
    optional field 6, UInt32 platformType
    optional field 0, cross-platform representation
    optional field 1, platform-specific representation
    terminator
```

Наблюдаемые platform type:

- `1` — legacy/cross-platform;
- `6` — Direct3D; встречается у 2 431 из 2 433 embedded Direct3D-объектов в
  каждом PC-корпусе, два legacy-объекта его опускают;
- `7` — поддерживается PC executable как альтернативная Direct3D-ветка, но в
  корпусе не встречается;
- `8` — PS2 native: 2 338 PS2-объектов;
- `9` — PS2 native с сохранённой cross-platform копией: четыре PS2-объекта;
- ещё 15 legacy PS2-объектов опускают field 6 и структурно остаются PS2 native.

## Direct3D BGRA32

Внутри platform-specific field 1 находится mip-stream:

```text
field 0, base mip:
    byte   present              // 1
    UInt32 width
    UInt32 height
    UInt32 pixelFormat          // 0 во всём корпусе
    byte   pixelDataPresent     // 1
    UInt32 mipWidth
    UInt32 rowStride            // mipWidth * 4
    UInt32 mipHeight
    byte   bgra[rowStride*mipHeight]

repeated field 1, following mip:
    UInt32 mipWidth
    UInt32 rowStride
    UInt32 mipHeight
    byte   bgra[rowStride*mipHeight]

terminator
```

Число mip-уровней задаётся количеством полей, а не отдельным счётчиком. В одном
pristine PC-корпусе встречаются 2 081 текстура с одним уровнем, 269 с двумя, 43
с тремя, две с шестью, десять с семью, 22 с восемью и шесть с девятью. Каждый
следующий уровень имеет `max(1, width/2)` и `max(1, height/2)`.

## PS2 native

Внутри platform-specific field 1 находится один field 0:

```text
byte   present                 // 1
UInt32 pixelFormat
UInt32 width
UInt32 height
UInt32 auxiliaryValue         // сериализован, семантическое имя пока неизвестно
UInt32 mipCount

palette:
    format 0:  16 * 4 bytes
    format 1: 256 * 4 bytes
    format 3:  no palette

repeat mipCount times:
    UInt32 descriptor0
    UInt32 descriptor1
    UInt32 descriptor2
    UInt32 dataSize
    byte   data[dataSize]
```

Логические размеры mip вычисляются делением исходных размеров на два. Размер
данных подтверждён для каждого из 2 357 PS2-объектов:

```text
format 0: ceil(mipWidth * mipHeight / 2)   // indexed 4-bit
format 1: mipWidth * mipHeight             // indexed 8-bit
format 3: mipWidth * mipHeight * 4         // 32-bit
```

Распределение pristine PS2:

| Format | Представление | Объекты | Палитра | Mip count |
|---:|---|---:|---:|---|
| 0 | indexed 4-bit | 644 | 64 байта | 1–5 |
| 1 | indexed 8-bit | 1 630 | 1 024 байта | 1–6 |
| 3 | 32-bit | 83 | нет | 1, 2 или 4 |

Размеры лежат в диапазоне от 8×8 до 512×512, включая прямоугольные 64×32,
128×64, 128×256, 256×128 и 512×256. Четыре служебных значения mip полностью
сохраняются в БД как `descriptor0..2` и `dataSize`; аппаратные имена первых трёх
пока не присвоены.

## Исправленная старая гипотеза

Старый decoder называл значения `0x0EE3`, `0x32E3`, `0x43E3`, `0x54E3` и
`0x29E3` форматами текстуры. Это неверно:

- младший байт `E3` — compact data-block header field 3 с 32-битным размером;
- следующий байт — младшая часть размера payload;
- поэтому число меняется от общего размера вложенного контейнера и mip-цепочки,
  а не от pixel format;
- байт по старому offset `+0x3C` в обычной Direct3D-форме является последним
  байтом `mipHeight`, а не отдельным нулевым serializer marker;
- настоящий pixel format хранится внутри representation: `0` для наблюдаемой
  Direct3D-формы и `0/1/3` для PS2.

`SmoTexture.FormatCode` временно сохраняет это двухбайтовое значение только как
legacy diagnostic signature для совместимости. Новый код не принимает решений
по нему.

## Executable и инструменты

Pristine PC executable содержит регистрации `spTextureData`,
`spTextureDataSerializer`, `spDXTextureDataSerializer` и
`spPS2TextureDataSerializer`. Class registration находится около RVA
`0x002D1D30`; фабрики сериализаторов — около `0x0042B660`, `0x0042C9E0` и
`0x0042DC30`. Writer-код независимо подтверждает field IDs источника,
cross/native-представлений и platform type. PS2 executable содержит общий и
PS2 serializer с теми же именами полей.

`SmoTextureDataDecoder` строго ограничивает все вложенные чтения объектной
записью и возвращает обе representations, палитру и mip metadata.
`SmoTextureDecoder` выбирает cross-platform BGRA либо Direct3D BGRA для
превью. Inspector показывает source kind, platform type, обе representations,
размер, настоящий format, bpp, палитру и mip count.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spTextureData
```

повторно читает исходные directory/PCK-ресурсы, проверяет все 7 485 объектов,
аннотирует прямые поля decoded JSON, записывает пять вариантов, 7 485 назначений
и четыре evidence-записи. Проверка идемпотентна.

Открыты точная семантика PS2 auxiliary/descriptors и подтверждённое
unswizzle/channel conversion для визуального превью. Запись палитр и нескольких
явных mip-уровней остаётся вне реализованного PC writer; готовность одиночного
PC BGRA mip не переносится на эти варианты или PS2.
