# 2026-08-01 — от «контейнера с моделью» к объектному графу

## Исходный вопрос

Нужно начать просмотрщик SMO и понять, является ли файл простым контейнером mesh или игра использует более сложную внутреннюю структуру. В качестве отправной точки доступны старый `SMOTextureTool`, несколько Samples, executable и рабочая папка игры. Рабочая папка ранее менялась экспериментами и не считается pristine.

## Инженерный результат

Созданы и связаны два направления:

- `SmoViewer`: строгий Core parser, CLI inspect/scan, synthetic/corpus checks и простое WPF-окно с добавлением объекта;
- `SMOTextureTool`: существующий texture decode/export/replace/repack workflow;
- `SparkplugEngineResearch`: общий workspace, документация и дневник поверх обоих Git submodule.

## Подтверждено

1. SMO начинается с little-endian заголовка `FFPS` размером `0x20`.
2. После заголовка расположен каталог переменных записей с именем, class hash, logical offset и serialized size.
3. Физический offset тела равен `DataStart + logicalOffset`.
4. После каталога присутствует нулевой terminator.
5. Обычное тело начинается с `typeHash + "SBOO"`.
6. `serializedSize` может охватывать поддерево; записи допустимо вкладываются.
7. Заголовок `spDataBlockSerializer` использует 5 бит type и 3 бита size code, включая расширенные длины.
8. Реестр содержит 17 подтверждённых class ID, то есть SMO описывает граф model/mesh/material/texture/node/skin/collision/shadow, а не единственный mesh blob.
9. Для mesh найдены `E0`/`E1`; primitive type `3` — triangle strip.
10. В `E1` runtime stride не всегда равен числу сериализованных байт на vertex.

## Решения parser

- Сначала валидировать FFPS и каталог, затем открывать тело объекта.
- Не искать mesh-сигнатуру по всему файлу как основной алгоритм.
- Не «чинить» stale offset молча: возвращать диагностический код и точную позицию.
- Не читать disk buffer с runtime stride, если структура подтверждает меньший serialized stride.
- Хранить unknown classes/fields, не присваивая им предполагаемые имена.
- Разделять pristine, modified и PS2 evidence.

## Baseline текущего корпуса

Строгий decoder распознал `24 039 / 25 395` объектов `spMeshData`.

| Неразобранная группа | Число |
|---|---:|
| stale offset / signature mismatch | 680 |
| PS2 preamble variant | 494 |
| PS2 boundary variant | 137 |
| primitive type `2` | 45 |

Это измерение частично изменённой рабочей папки. Оно пригодно как regression baseline, но не доказывает частоту вариантов в оригинальной игре.

## Связь со старым TextureTool

Старый проект остаётся ценным вторым источником: известны texture format codes, ABGR/BGRA layouts, material state arrays, vertex diffuse modulation и работоспособная замена HD-текстур. Одновременно обнаружено, что ранний repack после изменения размера мог обновлять общие длины, но оставлять stale offsets/sizes последующих записей каталога. Это объясняет часть расхождений и задаёт требование catalog-safe repack.

## Что не установлено

- смысл заголовочных полей `0x08` и `0x10`;
- структура primitive type `2`;
- точные PS2 `E1` preamble/boundary layouts;
- все vertex declarations/FVF;
- transform hierarchy, winding/coordinates и полная material semantics;
- skin, animation и world-format связи.

## Следующий эксперимент

Найти чистую PC-сборку, получить отдельный PS2-корпус, записать SHA-256 manifests и повторить scan той же ревизией parser. Затем выбрать минимальные примеры четырёх нерешённых diagnostic groups и закрывать их по одному synthetic test на вариант.
