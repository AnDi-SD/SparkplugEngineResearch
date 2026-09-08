# Общий FFPS/FAT reader для C# инструментов

Следующий этап цикла до 07:30 МСК. `SmoDocument` удалил самостоятельный разбор
фиксированного заголовка, таблицы объектов и object signatures. C# вызывает
общую реконструкцию через C ABI. Ранее перенесённый `SmoDataBlockReader` уже
использовал `spDataBlockSerializer`; повторная его реализация не понадобилась.

## Реализация и граница

`spSerializerManager::ReadAndValidateHeaderForAnalysis` читает исходные семь
слов и возвращает настоящий результат header validation. Чтение записей FAT
выделено из `spResourceFATHelperForAnalysis::LoadIndexForAnalysis` в общий
`ReadIndexEntriesForAnalysis`: прежний loader через visitor выполняет свои
RTTI/duplicate-ID проверки **сразу после каждой записи**, с прежним порядком
и остановкой на первой ошибке. Реконструированный алгоритм не исправлялся.
Отдельное наблюдение позиций даёт table/name offsets и точную длину имени,
включая различие null и непустого указателя на пустую строку.

Чтение восьми байтов object header также общее: `ReadObjectHeaderForAnalysis`
используется и `ReadObjectHeaderAndCreateForAnalysis`, и инспектором.
`HasCanonicalObjectMarkerForAnalysis` определяет канонический SBOO. Настоящий
runtime по-прежнему не проверяет этот marker в factory path — дополнительная
диагностика инспектора не выдаётся за поведение игры.

Host API `spv_container_inspect/info/entries/destroy` возвращает batch metadata.
Входной массив заимствуется только на один синхронный вызов, без второй копии
SMO; native handle хранит только числовые результаты. C# через SafeHandle
копирует весь массив записей одним вызовом и освобождает handle. Размеры обоих
ABI structs36 bytes. Исходные байты остаются собственностью `SmoDocument`.

Это **просмотр метаданных**, а не полноценная загрузка неизвестных классов.
Инспектор может показать unknown class ID без создания runtime object и без
подстановки пустого класса. `spv_graph_load` по-прежнему требует реальных
RTTI/factories/readers всех ресурсов. Wrong-version/platform metadata можно
просмотреть; `SmoHeader.NativeValidationStatus` сохраняет результат оригинального
validator. Отсутствующий runtime reader не маскируется успешной загрузкой.

Собственные обязанности C#: диагностика диапазонов/перекрытий, декодирование
имени для отображения (UTF-8/Latin-1), исходные byte slices и рабочие сценарии.
Это не вторые игровой serializer или FAT parser. Typed resource decoders,
операции редактирования и часть scene adapters ещё требуют миграции.

Ограничения host inspection:36 bytes..64 MiB, максимум65 536 FAT entries,
таблица заканчивается точно перед четырёхбайтовым нулевым file-index count;
ненулевой external-file index пока явно отклоняется. Имена не читаются за
пределами таблицы. Сохраняется прежний отказ C# на имя без конечного NUL.
Теперь каталог без отображения также требует native DLL и Windows x64;
такова текущая платформенная граница общей C# обвязки.

## Проверки

Сборки C++ DLL и C# Core/Editing/Corpus/FormatTests прошли. Семь прежних выбранных
C++ suites и отдельно расширенный FullLoader suite (213 checks) прошли.
Последний проверяет, что raw inspection видит неизвестный ID, а runtime FAT
останавливается ровно после него, сохраняя предшествующую запись и положение
stream. Загрузки общего resource graph на меню, tile_bad и Bloom также прошли.

C# FormatTests на одном `igmenu_opt_pc.smo`:9 262 assertions, включая новые
проверки неизвестного ID, original header status, source offsets, malformed
table/name/count, signature diagnostics и восстановления после отказа.
После нормализации исходников и финальной сборки отдельный C# прогон на
`igmenu_opt_ps2.smo` прошёл2 937 assertions. UI/GPU этим не проверялись.

[`validate_tools_container_index.py`](../../research/validate_tools_container_index.py)
сверил все FAT identities/offsets/sizes/raw name extents с уже существующей
read-only базой для точно совпадающих SHA256 пяти файлов:

| Файл | Objects | Platform mask | Native metadata bytes |
|---|---:|---:|---:|
| Bloom/bloom_jeans |121|2|4 356|
| igmenu_opt_pc |1 225|2|44 100|
| tile_bad |127|2|4 572|
| igmenu_opt_ps2 |342|9|12 312|
| mmenu_new_ps2 |1 167|8|42 012|

Native inspection с копированием descriptors заняла0,53..4,43 ms в этом запуске.
Замер не включает чтение файла и сравнение всех C# диагностик. PS2 payloads
проверены как данные; PS2 executable/GPU не запускались. Полного corpus scan нет.

Для дальнейшей адресной проверки `Build-Native.ps1 -RunChecks -CheckSuites FullLoader`
собирает/запускает только выбранный suite. Выбор не расширяет corpus и не меняет
игровой алгоритм; без параметра сохраняется обычный набор доступных suites.
