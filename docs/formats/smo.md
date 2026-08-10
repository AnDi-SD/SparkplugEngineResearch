# Формат SMO

Статус: черновая спецификация, основанная на текущем parser и локальном частично изменённом корпусе. Техническая запись рядом с реализацией находится в [`tools/SmoViewer/docs/SMO_FORMAT.md`](../../tools/SmoViewer/docs/SMO_FORMAT.md).

## Короткий ответ

SMO — это little-endian `FFPS`-контейнер сериализованного объектного графа Sparkplug, а не просто архив одного mesh. В одном файле могут быть модели, несколько мешей, материалы, текстуры, scene nodes, skin/collision и вспомогательные render-объекты. Каталог связывает имена и class ID с сериализованными областями данных; движок восстанавливает из них runtime-объекты.

Последнее предложение подтверждено формой каталога и набором зарегистрированных классов, но точный порядок создания объектов, владение и разрешение ссылок ещё требуют проверки по executable/runtime.

## Заголовок FFPS

Заголовок занимает `0x20` байт:

| Offset | Тип | Текущее значение/смысл |
|---:|---|---|
| `0x00` | `char[4]` | `FFPS` |
| `0x04` | `UInt32` | наблюдалось `0x26` |
| `0x08` | `UInt32` | неизвестно |
| `0x0C` | `UInt32` | объявленный размер файла |
| `0x10` | `UInt32` | вариант; наблюдались `1`, `2`, `3`, `8`, `9` |
| `0x14` | `UInt32` | абсолютный `DataStart` |
| `0x18` | `UInt32` | `DataSize` |
| `0x1C` | `UInt32` | `ObjectCount` |

Для корректного исследованного файла ожидается:

```text
DataStart + DataSize == FileSize == actual file length
```

Варианты `8` и `9` наблюдались у ресурсов с суффиксом `_ps2`. Называть поле `0x10` версией формата пока преждевременно.

## Каталог объектов

Каталог начинается с `0x20`. Запись имеет переменную длину:

```text
UInt32 entryMarker
UInt16 nameByteCount
Byte[nameByteCount] zero-terminated name
UInt32 typeHash
UInt32 logicalOffset
UInt32 serializedSize
```

После последней записи следует нулевой `UInt32`. Адрес тела вычисляется строго:

```text
physicalOffset = DataStart + logicalOffset
```

`serializedSize` может включать поддерево. Интервалы каталога могут не пересекаться или полностью вкладываться друг в друга; вычислять размер как расстояние до следующего logical offset нельзя.

## Тело объекта и data block

Обычное тело начинается так:

```text
UInt32 typeHash
char[4] "SBOO"
... serialized fields ...
```

В первом байте поля `spDataBlockSerializer` младшие пять бит задают field type, старшие три — size code:

| Size code | Размер payload |
|---:|---:|
| `0` | 0 |
| `1` | 1 |
| `2` | 2 |
| `3` | 4 |
| `4` | 8 |
| `5` | следующий `UInt8` |
| `6` | следующий `UInt16` |
| `7` | следующий `UInt32` |

Если пятибитный type равен `0x1F`, настоящий type находится в следующем байте.

Подтверждённые hash/name пары вынесены в [реестр class ID](../reference/class-ids.md). Parser сверяет hash записи каталога с hash в теле и сообщает расхождение, но не перемещает объект эвристически.

## Актуальная проверка PC-корпуса 2026-08-10

`SmoViewer.FormatTests` на `local-data/pc-pristine/Media` успешно обработал 416/416 SMO: 177 369 записей object directory и 22 012 mesh без падения strict parser (140 269 assertions). Это актуальный baseline чистого PC-корпуса. Приведённые ниже числа прежнего смешанного/частично изменённого scan сохранены только как историческая диагностическая выборка и не должны подменять этот baseline.

## Mesh data

Текущий строгий decoder подтверждает:

- варианты сериализации, условно названные `E0` и `E1`;
- primitive type `3` как triangle strip;
- позицию из трёх `Single` с offset `0` для известных layouts;
- необходимость различать serialized stride и runtime vertex-buffer stride;
- необходимость определять границы из структуры объекта, а не глобального поиска байтовой сигнатуры.

Известные примеры различия stride:

| Ресурс/layout | Runtime stride | Serialized stride |
|---|---:|---:|
| `fish.smo`, `0x093E` | 56 | 44 |
| `bloom_ball.smo`, `0x197E` | 76 | 64 |
| `loading.smo`, `0x0940` | 36 | 36 |

Не закрыты primitive type `2`, несколько PS2-вариантов `E1`, часть boundary cases и семантика всех vertex layouts.

## Материалы и текстуры

`SMOTextureTool` даёт независимые инженерные подтверждения:

- встречаются известные ABGR/BGRA pixel layouts;
- текстура находится в графе material/layer/texture objects;
- для корректного preview требуется учитывать vertex diffuse modulation;
- repack без изменений может быть byte-identical;
- увеличение отдельных текстур до 1024/2048 проверялось запуском игры.

Однако ранние эксперименты при изменении длины pixel buffer обновляли общие `FileSize`/`DataSize`, но не все последующие записи каталога. Поэтому signature scan полезен как восстановительный инструмент, но не заменяет корректный object parser и catalog-safe repack.

## Результат текущего scan

На локальном частично модифицированном корпусе строгий decoder обработал `24 039` из `25 395` объектов `spMeshData`. Остальные группы диагностированы как:

| Группа | Количество |
|---|---:|
| stale offset / signature mismatch | 680 |
| PS2 preamble variant | 494 |
| PS2 boundary variant | 137 |
| primitive type `2` | 45 |

Это baseline конкретного корпуса, не статистика всех игр или всех SMO. После получения чистой сборки scan должен быть повторён с manifest.

## Уточнения корпуса 2026-08-10

- `spNode`-иерархия skeleton восстанавливается по `esfNodeChild` (field type `5`), а не по каталожному `ParentIndex`, который описывает владение сериализованными интервалами.
- PC-layouts `0x097E` и `0x197E` хранят четыре `float32`-веса по `+12` и четыре байтовых индекса локальной bone palette по `+28`; `spSkin` отдельно хранит palette и inverse-bind matrices.
- В исследованном PC-корпусе palette имеет 16 slots, в PS2-корпусе — 64. Независимый Kikko rig использует для тела PC palettes `16 + 9`, тогда как PS2 хранит те же 20 уникальных костей в одной palette. Автоматическое разбиение exporter'ом остаётся сильной гипотезой.
- PS2 `E1` содержит platform-specific DMA/VIF representation: для 973 mesh Gardenia01 выполняется `payloadSize = 0x28 + dmaQwordCount * 16`; первые четыре `float` задают bounding sphere.
- PS2 `E2` в Gardenia01 имеет длину 24 байта и соответствует `esfMeshDataBoundingBox` (`minXYZ`, `maxXYZ`).
- `menu.smo` — GUI scene. Button-state meshes используют layout `0x0100` (`XYZ + Diffuse ARGB`, без UV), а текст представлен `spTextNode`, `spTextRenderable` и `spFont`.
- Наличие `Is32Bit()` в serializer подтверждает архитектурную поддержку 32-bit index buffers. Значение 65 535 нельзя считать доказанным общим лимитом Sparkplug.

## Реализации

- [`SmoDocument`](../../tools/SmoViewer/SmoViewer.Core/SmoDocument.cs) — заголовок и каталог.
- [`SmoClassRegistry`](../../tools/SmoViewer/SmoViewer.Core/SmoClassRegistry.cs) — известные class ID.
- [`SmoViewer.Inspect`](../../tools/SmoViewer/SmoViewer.Inspect/Program.cs) — человекочитаемый/JSON scan.
- [`SmoViewer.FormatTests`](../../tools/SmoViewer/SmoViewer.FormatTests/Program.cs) — synthetic и corpus checks.
- [`SMOTextureTool.Core`](../../tools/SMOTextureTool/SMOTextureTool.Core) — texture decode/repack.

Любое расширение спецификации сначала должно давать строгую диагностику на неизвестном варианте и только затем добавлять decode. Молчаливое угадывание offsets затрудняет проверку гипотез.
