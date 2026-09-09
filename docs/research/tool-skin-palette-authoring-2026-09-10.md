# Общая запись Skin palette с исходными Node ID

Шестой блок цикла, первоначально назначенного до07:00 МСК и сокращённого
пользователем до23:00 МСК 9 сентября. Importer формировал Skin palette в C#:
заголовок, weights/count, пары ID/size и 64 байта каждой матрицы. Поле искалось
по совпадению нулевого weights и boneCount среди всех секций. Вместо этого
используются общий `spSkinSerializer` и точное наблюдение actual Skin reader.

## Подтверждённый контракт

Существующая запись PC `490DA0` выделена в
`spSkinSerializer::WritePaletteFieldWithContextForAnalysis`. Полный writer
вызывает тот же helper. Он пишет одно UInt32-length field0, raw weights/count,
настоящий `spSerializer::WriteReferenceForAnalysis` и raw64B inverse-bind matrix
для каждого actual Node. Helper не добавляет terminator. Пустая палитра
сохраняет прежнюю возможность записи без manager; непустая её не получает.
Это выделение уже восстановленного кода, не исправление игрового алгоритма.
Эталон — [CP62](native-pc-skin-serialization.md) и ранее сохранённые original
empty/raw-bits/repeat-field captures. Владение костями проверено в
[CP107](native-pc-whole-skin-scene.md).

Две свежие PC micro-пробы отдельно проверили сохранение ID7 и ID1373:
actual Node/serializer factories и FAT indexer `466FA0` получают явно заданный
nextID, строят обе карты/list; actual reference writer `467350` при явно заданном
retained payload state пишет ровно `[ID,0]`. На случай: 8 байт, 2 writes,
0 seeks, без Node/header writer. Всего40checks, cleanup9/9 allocations,
heap5040B, максимальный вызов31723 инструкции. Лимиты100k/2s, heap64KiB,
single allocation32KiB, внешний child30s не повышались; stopped guest нет.
Startup и retained offset/size/flag — явные входы, не доказанный whole save.

`spResourceFATHelperForAnalysis::SetNextResourceIDForAnalysis` — явно наш setter
такого входного состояния. Он допускает только ID1..FFFFFFFE и монотонное
продвижение nextID; обе карты и indexing остаются прежними. Отклонённый запрос
не меняет FAT. Это не новый индексатор и не восстановленное имя метода игры.

## Подключение

`spv_skin_write_palette` принимает actual traced ResourceGraph, raw weight word
и до1024 пар Node ID/матрица. Он проверяет exact Node wire class, явного владельца
и полное чтение Node по его FAT extent. Cache/skip не заменяет coverage. Разные
IDs одного указателя отклоняются; повтор одного ID с разными матрицами допустим.
Сортировка уникальных ID используется только для подготовки FAT, порядок
слотов палитры сохраняется. Матрицы не инвертируются и не нормализуются.

Один `SmoSkinPaletteWriter` владеет графом на время всех новых ветвей одного
Inject; до вставки и итоговой проверки он освобождается. Временный actual Skin
заимствует кости этого графа. Приложение сохраняет исходные Node IDs, transforms
и hierarchy в контейнере. Байты enclosing Node могут менять nested sizes при
вставке; безусловное побайтное сохранение каждого Node payload не заявляется.
Writer не сохраняет FAT/целый контейнер и не перемещает объекты.

Actual Skin reader сообщает отдельные host rows: headerOffset, payloadOffset,
payloadSize, assignmentOrder относительно всего Skin payload без8B object header.
Renderable/Model field0 в них не попадают. Повторные поля в игре остаются
last-wins, все их locations наблюдаемы. C# decoder передаёт эти DTO; partial
inspection не создаёт фиктивные кости.

`BuildReferencePaletteField` теперь только выбирает имена/actual ID и матрицы,
после чего вызывает общий writer. Padding до16 повторением первой кости и
weights0 — прежняя явная packing policy инструмента, не алгоритм serializer.
Ручные ID/matrix/count writes и `FindPaletteField` удалены.

## Границы и проверка

Редактирование требует ровно одного actual palette field с каноническим
UInt32 заголовком. Repeated/missing, иная reservation и forced-extended field0
дают `SKIN_PALETTE_SHAPE`; исходный reader эти формы не теряет. Редкое lossless
сохранение заголовков остаётся ранее отложенным редакторским случаем.

Запись теперь требует успешной загрузки всего исходного graph, даже если сами
нужные Node читаются отдельно. Действуют64MiB/8192-object и остальные существующие
границы ResourceGraph. Неподдерживаемые ресурсы — явный отказ и задача точечного
исследования; старый C# serializer не служит fallback. Предложение для общего
[FAT/envelope encoder](tool-container-writer-boundary-2026-09-10.md) остаётся
отдельным и в этом блоке не реализуется.

Native проверка завершена: SkinSerialization243, SaveReference110,
ReadReference298, FullLoader213, ReferenceReadTrace61. Сборка и CTest заняли
60,83с с одним worker. Общий Build-Native теперь имеет явный BuildWorkers1..4,
default2; для этого блока выбран1 из-за доступной RAM. Пять consumer projects
собраны без ошибок и предупреждений, также с одним worker. Managed palette35,
Renderable scalar44 и FormatTests647 прошли. Icy end-to-end13 дал прежние
101 объект/13110B побайтно: SHA256
`63D554FE8C3149DC5113F53FA4A1421507DA5F9D964BB25DDF2B74D6D8944003`.
Palette regression:1,799с внутри процесса, peak32,39MiB.

Независимая ABI проверка:6 записей с повторным вызовом,14 guards и4 проверки
расположения полей;0,490с. Проверены17 original dependencies, pristine EXE
и sealed captures. Три реальные выборки Icy/Knut/Goopmonster содержали24 Skin
с weights0; nonzero weights подтверждены original/synthetic, а не ошибочно
приписаны этим файлам. Alias и uncovered-Node guards просмотрены в коде, но
публичный ABI harness не создаёт такие actual graphs; их динамическое покрытие
не заявляется. Локальные артефакты: `authoring-palette/`,
`authoring-palette-prebind/` внутри `local-data/results/tools-core-cycle-20260910-0700/`.
