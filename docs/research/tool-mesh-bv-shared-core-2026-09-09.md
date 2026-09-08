# MeshBV и wxFaceData в общих C# cores

Шестой блок цикла до 07:30 МСК 9 сентября. `SmoMeshBoundingVolumeDecoder`
теперь получает геометрию и face records из настоящих восстановленных классов
`spMeshBV`, `spCollisionMesh`, `spIndexBuffer`, `spVertexBuffer`,
`spFaceDataContainer` и `wxFaceData` через общий C ABI. Собственные C# readers
этих payloads удалены. Это изменение общего Core, используемого вьювером,
редакторами и экспортёрами; интерфейс не перерабатывался.

## Источник и граница

Оригинальные методы, fixtures и ограничения описаны в
[предыдущем досье](native-pc-mesh-bv-tools-core-2026-09-09.md).
Новых заявлений о collision queries или OPCODE tree этот блок не делает.
Пять оригинальных micro cases и семь sphere comparisons из прошлого блока
сохраняются как исторические свидетельства; заново оригинальный EXE здесь
не исполнялся. Существующие C++ golden tests проверены после выделения общей
ветки чтения геометрии.

`spMeshBVSerializer::ReadGeometryForAnalysis` содержит прежнюю последовательность
IB.Read → VB.Read. Её вызывают и whole-resource serializer, и прямой инспектор
поля. Это именованный доступ к той же восстановленной ветке, без второй грамматики.
Чтение целого MeshBV payload выполняет сам `spMeshBVSerializer`; чтение массива
faces — сам `spFaceDataContainer` с настоящей RTTI-фабрикой wxFaceData.
Эти leaf payloads не содержат FAT resource references. Для их инспекции не нужно
загружать остальные ресурсы файла или создавать неизвестные классы.

Добавлены только наблюдения за выполненным чтением: маски присутствовавших
полей и позиция массива вершин. Они явно помечены как host metadata, не являются
членами оригинального layout и не добавлены в native CopyFrom/Clone. Смещение
использует фактическую позицию stream и размер прочитанного VB. Редактор получает
его при первом чтении MeshBV и больше не разбирает геометрию повторно.

## Отличия от прежнего приложения

- wxFaceData пропускает неизвестные поля и применяет повторные по порядку,
  как PC5A5320; C# больше не запрещает эти случаи.
- Число записей spFaceDataContainer независимо от количества треугольников.
  Старый аргумент `expectedFaceCount` сохранён для совместимости вызовов,
  но не вводит отсутствующую в игре проверку.
- Первое UInt32 геометрии — primitive type IB, не версия. Новый `PrimitiveType`
  передаёт это явно; старые DTO `Version` и `CurrentVersion` оставлены как
  подписанные совместимые имена, чтобы не переписывать UI в этом блоке.
- Пустая геометрия, неконечные позиции, неверные индексы, нарушение extent и
  неподдерживаемый тип остаются явными host guards общего ограниченного reader.
  Их не выдаём за оригинальную валидацию. Доступ к OPCODE API не подменён успехом.
- Названия поверхностей в C# остаются подписями инспектора к числовым значениям,
  без реализации игровой реакции на поверхность.

## ABI и lifetime

Добавочные функции ABI2: `spv_mesh_bv_read`, `destroy`, `info`, `geometry`, `faces`.
Режимы входа: 0 — целый поток полей без SBOO header, 1 — поле geometry,
2 — поле faces. Вход 1 byte..16 MiB, синхронно заимствуется pinned span.
Handle владеет восстановленными объектами; C# SafeHandle освобождается сразу
после batch копирования DTO arrays. Ни указатели на исходный SMO, ни native
указатели на вершины в приложении не сохраняются. Общий mutex защищает RTTI.

## Проверка

- C++ MeshBVCore: **83 checks**, включая старые original writer bytes,
  unknown/repeated fields, наблюдаемые offsets/masks и отсутствие их в clone.
- C# FormatTests: **9 271 assertions** на `igmenu_opt_pc.smo`, **2 946** на
  `igmenu_opt_ps2.smo`, **1 504** на `SFX/tile_bad.smo` с настоящими face data.
  Каждый запуск включает синтетику: standalone/whole geometry agreement,
  original-PC unknown/repeated golden record, независимый count, явный zero,
  truncation и повторное открытие после отказа. Это три выбранных файла,
  не полный corpus и не проверка работающего PS2 EXE.
- Whole resource graphs меню (**1 225 objects**) и tile_bad (**127**) сохранили
  metadata, владельцев и Node relations после выделения общего helper.
- Контрольные сборки C++ и C# прошли. Известное предупреждение C4756 sphere
  producer не исправлялось изменением оригинальной арифметики.

Первый запуск graph validator указал неверный путь третьего файла Bloom и
остановился до сохранения отчёта. Повторная завершённая выборка содержит только
два вышеуказанных файла. Частичный запуск не посчитан успешным третьим тестом.

Локальные логи и graph report:
`local-data/results/tools-core-cycle-20260909-0730/mesh-bv-bridge/`.
Снимок зависимостей: `research/tools-core-mesh-bv-bridge-block-2026-09-09.json`.
Остальные C# typed resource readers и часть writers ещё требуют переноса.
Следующий приоритет — render mesh buffers, которые нужны всем экспортёрам.
