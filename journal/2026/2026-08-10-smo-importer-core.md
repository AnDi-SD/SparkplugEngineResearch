# 2026-08-10 — первое ядро безопасного импорта

## Цель

Проверить минимальную замену геометрии без изменения object graph, каталога FFPS и размеров `spMeshData`, привязав все вершины к одной существующей palette bone.

## Реализация

Добавлены `SmoImporter.Core`, WPF GUI и format checks. Reader принимает GLB 2.0 и OBJ. GUI показывает выбранный исходный mesh серым фоном и replacement оранжевым, позволяет выбрать target/replacement mesh, palette slot и задать uniform scale, XYZ rotation и translation.

Writer работает только при совпадении vertex count и точного triangle order. Оригинальный triangle strip сохраняется. В существующие vertex records записываются positions, доступные normals/UV0 и rigid skin data: weights `(1,0,0,0)`, первый index — выбранный palette slot. Исходный SMO никогда не перезаписывается; новая копия после записи повторно открывается strict parser.

## Проверка

На первом mesh `bloom_jeans.smo` выполнен SMO → GLB → SMO identity round-trip: 3255 vertices, исходная длина файла и strip indices сохранены, позиции совпали с допуском `1e-4`, все веса стали rigid, исходный SHA-256 не изменился.

## Ограничение

Это ещё не arbitrary topology import. Blender может перестроить порядок вершин/triangles даже без визуального изменения; такой файл writer отклонит. Следующий этап — полный catalog-safe repack и управляемая перекодировка topology.

## Разбиение целой модели

Добавлены объединение всех meshes входной OBJ/GLB-сцены и детерминированный `MeshSplitter`. Он режет поток только между triangles, создаёт локальные индексы, переносит normals/UV и при необходимости дублирует вершины на границах chunks. Политика настраивает пределы vertices, indices и triangles; значения по умолчанию соответствуют консервативному UInt16-профилю: 65 535 vertices, 65 535 indices и 21 845 triangles.

GUI показывает число chunks и сравнивает его с числом существующих mesh slots исходного SMO. Сохранение split-плана намеренно отключено: размещение chunks с новой topology требует catalog-safe repack таблиц, offsets и sizes.

На `bloom_jeans.smo` format checks расширены до 12 assertions: каждый chunk соблюдает заданные малые тестовые лимиты, суммарное число triangles сохраняется, повторный запуск даёт те же локальные индексы.

## Эксперимент Shrek → Bloom

Реализован whole-model repack существующих `spMeshData` slots. Writer создаёт E1 triangle-list payloads формата `0x97E`, пересчитывает FFPS logical offsets, serialized sizes, размеры охватывающих объектов и заголовок файла. Каждый chunk переводится из glTF world space в local space соответствующего Bloom mesh и получает rigid weights на первую подтверждённую bone его skin palette.

`local-data/shrek.glb` содержит 17 655 исходных вершин и 29 789 triangles. Модель распределена по шести mesh slots `bloom_jeans.smo`; после дублирования вершин на границах получилось 18 047 локальных вершин. Создан `local-data/bloom_jeans_shrek.smo` размером 1 485 071 байт. Strict parser повторно декодировал 6/6 meshes без diagnostics и подтвердил сохранение всех triangles и rigid weights (17 assertions).

Это пока структурная, а не игровая валидация. Материалы и две встроенные текстуры остаются от Bloom, collision/skeleton/object graph не заменяются. Следующая обязательная проверка — загрузка копии в игре; исходный pristine-файл не изменялся.

## Результат первой игровой проверки

Игра загрузила six-slot вариант без crash, а animation graph продолжил работать. Однако chunks двигались как жёсткие части исходной Bloom: последовательное разбиение triangles по шести старым skin/render branches визуально разорвало Shrek во время анимации. Эксперимент подтвердил работоспособность repack и triangle-list, но опроверг стратегию «один произвольный chunk на каждый старый slot».

Исправленный режим учитывает, что UInt16 ограничивает значение индекса, а не полную длину index buffer. У Shrek 17 655 уникальных вершин, поэтому все 89 367 индексов размещены одним цельным rigid mesh в крупнейшем body-slot. Остальные пять slots содержат по одной штатной вершине `0x97E`, но ноль triangles: это сохраняет подтверждённые stride/runtime headers и делает slots невидимыми.

Новый кандидат `local-data/bloom_jeans_shrek_rigid.smo`: 6/6 объектов декодируются, один содержит все 29 789 triangles, пять не содержат primitives; 19 assertions, FFPS diagnostics отсутствуют. Требуется повторная игровая проверка.

## Результат single-slot v1

Игра завершилась аварийно с `bloom_jeans_shrek_rigid.smo`, установленным как `pc-pristine/Media/Characters/Bloom/bloom_jeans.smo`. Файл в `pc-pristine` больше не является исходным; чистая копия сохранилась рядом как `bloom_jeans_old.smo` (613 109 байт) и используется как база следующих генераций.

V1 одновременно содержал две новые переменные: пять mesh с нулём primitives и один index buffer на 89 367 элементов. Поэтому crash сам по себе пока не различает запрет пустых render slots и скрытый runtime-лимит 65 535 элементов index buffer.

Создан диагностический `local-data/bloom_jeans_shrek_rigid_v2.smo`: основной slot по-прежнему содержит цельного Shrek, а каждый из пяти отключённых slots теперь имеет одну вершину и triangle `(0,0,0)`. Все шесть объектов сохраняют E1 `0x97E`, stride/runtime stride `56/68`; 19 assertions и strict parse проходят. Если v2 падает, следующая рабочая гипотеза — ограничение полного index buffer и необходимость направить несколько chunks в одну общую transform/bone ветку.

Игровая проверка V2 успешна: crash исчез. Следовательно, игровой renderer требует ненулевой primitive/index path для существующих render slots, а index buffer Shrek длиной 89 367 элементов поддерживается. V2-поведение перенесено в основной GUI writer. В GUI также добавлена автоматическая подгонка высоты и центра replacement по полным world-space bounds исходного SMO.

GUI расширен выбором rigid palette bone для всей видимой модели и необязательной заменой основного character atlas из PNG/JPEG. Texture path проходит через подтверждённый `SMOTextureTool.Core` repack после mesh repack; повторно проверяются FFPS-каталог, meshes и texture catalog. Выбранный bone slot записывается в первый blend index каждой вершины при weights `(1,0,0,0)`.

GLB-reader теперь следует `material.pbrMetallicRoughness.baseColorTexture` через `textures[].source` к embedded `images[].bufferView`, извлекает PNG/JPEG из BIN chunk и передаёт его в тот же texture repack. GUI показывает все найденные встроенные base-color images и автоматически выбирает первую; внешний PNG/JPEG остаётся ручным override.

Первоначальная защита с масштабированием входного изображения до старого atlas оказалась несовместимой с игрой. Она удалена. Importer теперь без изменений использует resize/repack `SMOTextureTool.Core`, сохраняя реальное разрешение embedded image, а после изменения длины файла дополнительно пересчитывает logical offsets и serialized sizes FFPS object directory. На `shrek.glb` получен файл 68 310 149 байт; texture catalog и 6/6 meshes повторно декодируются, проходят 23 assertions.

Первая игровая проверка версии с ручным bone завершилась crash. Сравнение с рабочим V2 показало конкретное отличие: видимый mesh был записан на blend index 8 вместо рабочего 0. GUI ошибочно предлагал синтетические fallback slots 0–15 при неполностью декодированной связи skin. Fallback удалён: теперь показываются только palette indices, реально использованные исходным host mesh при ненулевых weights; writer отклоняет любой иной slot до записи.

Игровая проверка exact embedded texture 4096×4096 также завершилась crash. Формат при этом не менялся: исходный atlas и replacement имеют `0x32E3`, layout ABGR; изменилась сторона с 256 до 4096 и pixel buffer вырос в 256 раз. В GUI добавлен предел 512/1024/2048/оригинал, по умолчанию 2048 — верхняя граница ранее успешных игровых экспериментов SMOTextureTool. Кандидат 2048×2048 имеет тот же format/layout, размер 17 978 501 байт и проходит 23 assertions.

Для изоляции причины crash в texture selector добавлен режим `Не менять текстуру исходного SMO`; он выбран по умолчанию. Embedded image или внешний PNG/JPEG применяются только после явного выбора пользователя.

SMOTextureTool показал, что primary body texture не имеет подтверждённого material owner, тогда как вторая 64×64 texture owner имеет. Resize первой до 1024×512 менял внутренние texture headers, но owner/container chain не могла быть пересчитана. Writer теперь применяет правило: texture без `MaterialReferenceInfo` заменяется только pixel-for-pixel при исходных width/height; входное изображение автоматически приводится к исходному размеру. Увеличение разрешения разрешено только texture с подтверждённым владельцем.

Дополнительная проверка показала, что owner основной texture восстанавливается нестабильно до/после combined mesh+texture repack. Поэтому whole-model importer больше не разрешает resize primary atlas вообще: выбранное embedded/внешнее изображение всегда приводится к исходным 256×256, формат остаётся `0x32E3`, layout ABGR и длина файла не меняется относительно mesh-only V2. Создан `bloom_jeans_shrek_texture_pixels_only.smo`, 23 assertions.

Побайтовое сравнение рабочего V2 и `texture_pixels_only` подтвердило: различаются только 196 149 значений внутри диапазона pixel buffer, вне него различий нет; material owner в обоих файлах — `material#1/pass1/layer1`. Потеря owner на скриншоте относится к более ранней resize-версии 1024×512. В writer добавлена обязательная проверка сохранения material/pass/layer association после texture repack.

После повторных игровых crash прежние выводы SMOTextureTool о работоспособности произвольной замены считаются неподтверждёнными. Texture import отключён в whole-model GUI. Создан минимальный игровой probe `local-data/bloom_jeans_texture_raw_one_byte.smo` непосредственно из чистого `bloom_jeans_old.smo`: XOR одного байта по адресу `0xECA`, внутри первого pixel buffer; все остальные байты, headers, каталог, geometry и размер файла совпадают с оригиналом. Оба parser-а принимают probe. Его игровая проверка отделит саму изменяемость texture bytes от ошибок decode/encode/repack.

Первый однобайтовый probe вызвал crash, но менял byte 0 ABGR-пикселя, то есть Alpha 255→254. Для исключения material alpha invariant добавлены probes, меняющие только B-канал при неизменном Alpha: первый пиксель и пиксель в центре atlas.

## Финальная игровая проверка texture writer

Оба исправленных однобайтовых RGB-probe успешно загрузились в игре. Это
подтвердило, что существующий pixel buffer изменяем и не защищён общей checksum;
первый crash нельзя использовать как доказательство запрета изменения текстуры,
поскольку тот тест затронул Alpha.

Следующий controlled file заменил весь RGB первого atlas изображением из
`shrek.glb`, но был построен непосредственно из чистого `bloom_jeans_old.smo`.
Изображение масштабировано до исходных `256×256`; каждый исходный Alpha-байт
сохранён, длина файла не изменилась, а вне pixel buffer побайтовых отличий нет.
Material owner остался `material#1/pass1/layer1`. Игра загрузила файл без crash и
показала новую текстуру.

Причина прежних вылетов локализована в resize/repack `SMOTextureTool`, а не в
формате `0x32E3`, квадратности изображения или самом изменении RGB. В
`SmoImporter` добавлен отдельный `FixedSizeTextureWriter`: он принимает embedded
GLB base-color либо PNG/JPEG, приводит RGB к размеру исходного atlas и не меняет
Alpha, headers, offsets или размер SMO. Этот путь включён в GUI и подтверждён
игрой совместно с whole-model rigid mesh replacement.

`SMOTextureTool` переведён в read-only режим: просмотр и экспорт остаются
доступны, а выбор замен и сохранение нового SMO отключены. Исторические заявления
о безопасных HD-размерах 1024/2048/4096 отозваны до новой независимой проверки.
