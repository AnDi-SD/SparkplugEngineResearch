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

Общая checksum файла в подтверждённых полях не обнаружена. Игровые RGB-пробы
показали, что данные можно менять без обновления отдельной checksum, если
согласованы размеры/offsets и не нарушен object graph. Поле `0x08` остаётся
неизвестным и не должно называться checksum без отдельного подтверждения.

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
- для `0x093E` — четыре `Single` blend weights с offset `12` и четыре локальных
  `UInt8` palette indices с offset `28`;
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

### Render flags `spMaterialData`

Первый подтверждённый data-block после сигнатуры материала имеет field type `3`
и payload `UInt32`. На проверенных материалах значение `0x2` означает обычный
непрозрачный проход, а установленный бит `0x4` (`0x6`) включает alpha blending.
Само наличие неоднородного alpha в текстуре не доказывает прозрачность: например,
обычный atlas `knut` содержит служебные alpha-значения при render flags `0x2`.

У `knutBoss.smo` материал `[10]` mesh `[13]` имеет flags `0x6`, а `gr_01`
содержит 211 полностью прозрачных, 746 полупрозрачных и 67 непрозрачных пикселей.
Материал тела `[26]` имеет flags `0x2`. Поэтому Viewer и Exporter определяют
blend mode по материалу, а не эвристикой по пикселям. В WPF прозрачная геометрия
добавляется после непрозрачной, иначе ранняя запись depth скрывает тело за щитом.

Rigid `spModel` без skinning может принадлежать анимируемому `spRenderNode`.
Такой mesh сохраняет собственный model world transform в bind pose, но при
воспроизведении/экспорте вычисляет локальный transform относительно ближайшего
render node и следует его SAN-треку. Подтверждённый пример — mesh `[6]` очков
`knut.smo`, связанный с `[2] Knut_TEMP_glasses`.

`SMOTextureTool` даёт независимые инженерные подтверждения:

- встречаются известные ABGR/BGRA pixel layouts;
- текстура находится в графе material/layer/texture objects;
- для корректного preview требуется учитывать vertex diffuse modulation;
- repack без изменений может быть byte-identical;
- writer/repack `SMOTextureTool` не считается совместимым с игрой: его файлы могли проходить внутренний parser, но вызывать crash;
- игровая проверка подтвердила замену только RGB-байтов внутри существующего fixed-size ABGR pixel buffer при сохранении исходного Alpha, headers, offsets и длины файла;
- исторические заявления об игровой проверке texture replacement 1024/2048 считаются опровергнутыми до нового независимого подтверждения.

Однако ранние эксперименты при изменении длины pixel buffer обновляли общие `FileSize`/`DataSize`, но не все последующие записи каталога. Поэтому signature scan полезен как восстановительный инструмент, но не заменяет корректный object parser и catalog-safe repack.

Практический безопасный writer находится в `SmoImporter`: входной PNG/JPEG или
embedded GLB base-color масштабируется до исходных размеров atlas, после чего
перезаписываются только R/G/B существующего ABGR-буфера. Alpha каждого пикселя,
все служебные поля, object graph и размер файла сохраняются побайтно.

Изолированный игровой тест 2026-08-13 уточнил это ограничение: pristine geometry
с заменённым RGB загружается, а тот же файл с заменой Alpha завершается аварийно.
Новая topology и skinned palettes при исходной текстуре загружаются. Поэтому
перенос Alpha из внешнего GLB/FBX не является поддерживаемой операцией, даже если
контейнер проходит strict parser и размеры texture leaf не меняются.

Игровой тест опроверг достаточность catalog-safe texture repack: вариант
`Faragonda.smo → bloom_jeans.smo`, где две группы `64×64` были объединены в
структурно корректный `128×64` leaf с пересчитанными catalog/object/reference
sizes, проходил strict parser и оба format test, но вызывал crash игры. Значит,
внутри runtime существуют дополнительные ограничения на texture object/layout,
которые ещё не выражены в известных FFPS-полях.

Следующий эксперимент — перенос полного donor render graph и отдельное добавление
известной service-ветви target — также вызвал crash, включая ранее работавшие пары.
Это доказывает, что выделенного набора collision/control objects недостаточно:
неизвестные target bindings должны сохраняться вместе с исходными object IDs.

Поэтому активный SMO → SMO writer сохраняет весь target graph и заменяет только
существующие mesh/texture leaves и reference-only palettes. Если donor требует
больше texture groups, операция блокируется. Будущий writer должен добавлять новые
visual branches внутрь сохранённого target graph после полного описания ссылок, а
не заменять target graph целиком.

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
- `bloom_jeans.smo` подтверждает распределение одного 95-node skeleton между шестью локальными PC palettes: основной body mesh не содержит arm/hand bones, которые находятся в четырёх дополнительных skin parts. Palette slot не является глобальным bone ID; надёжное объединение выполняется по node object ID.
- PS2 `E1` содержит platform-specific DMA/VIF representation: для 973 mesh Gardenia01 выполняется `payloadSize = 0x28 + dmaQwordCount * 16`; первые четыре `float` задают bounding sphere.
- PS2 `E2` в Gardenia01 имеет длину 24 байта и соответствует `esfMeshDataBoundingBox` (`minXYZ`, `maxXYZ`).
- `menu.smo` — GUI scene. Button-state meshes используют layout `0x0100` (`XYZ + Diffuse ARGB`, без UV), а текст представлен `spTextNode`, `spTextRenderable` и `spFont`.
- Наличие `Is32Bit()` в serializer подтверждает архитектурную поддержку 32-bit index buffers. Значение 65 535 нельзя считать доказанным общим лимитом Sparkplug.

## Связанные PC-анимации SAN/ANM

SAN использует тот же FFPS-контейнер, но хранит один animation object
`0x56EE563A`: duration и именованные position/rotation/scale curves. Curve
содержит служебный `UInt32=1`, `keyCount`, массив времени и затем Vector3 либо
quaternion XYZW. ANM является текстовой восьмиколоночной таблицей состояний со
ссылкой на SAN в последней колонке. На каталоге Bloom подтверждены 168/168 SAN;
подробный layout записан в
[`tools/SmoViewer/docs/SMO_FORMAT.md`](../../tools/SmoViewer/docs/SMO_FORMAT.md).

## Реализации

- [`SmoDocument`](../../tools/SmoViewer/SmoViewer.Core/SmoDocument.cs) — заголовок и каталог.
- [`SmoAnimationDecoder`](../../tools/SmoViewer/SmoViewer.Core/SmoAnimationDecoder.cs) — PC SAN curves.
- [`SmoClassRegistry`](../../tools/SmoViewer/SmoViewer.Core/SmoClassRegistry.cs) — известные class ID.
- [`SmoViewer.Inspect`](../../tools/SmoViewer/SmoViewer.Inspect/Program.cs) — человекочитаемый/JSON scan.
- [`SmoViewer.FormatTests`](../../tools/SmoViewer/SmoViewer.FormatTests/Program.cs) — synthetic и corpus checks.
- [`SMOTextureTool.Core`](../../tools/SMOTextureTool/SMOTextureTool.Core) — texture decode/repack.

Любое расширение спецификации сначала должно давать строгую диагностику на неизвестном варианте и только затем добавлять decode. Молчаливое угадывание offsets затрудняет проверку гипотез.
