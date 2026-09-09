# Цикл ядер tools до 07:30 МСК, 9 сентября 2026

Начат 8 сентября около 22:15 МСК. Срок отчёта: **2026-09-09 07:30 Europe/Moscow**
(`2026-09-09T04:30:00Z`). Статус: остановлен по просьбе пользователя после сообщения о перезагрузке.

Цель работы — ядра всех приложений `tools/` по новым правилам. Совместимость UI
учитывается отдельно; серьёзная переделка UI не задерживает ядро. Исследование
сначала закрывает конкретные недостающие методы/поля используемых классов.
Если ядра готовы раньше срока, цикл продолжается последовательным разбором EXE.

## Итог остановки — 09.09.2026 07:57:51 МСК

Последний сохранившийся исследовательский результат до сообщения пользователя:
**09.09.2026 05:39:36 МСК** (mtime `static-matrices/scout-with-renderer.log`).
Последний завершённый блок ядра закоммичен в **05:34:55 МСК**, `3b23bc1`.
Точное время аварийного прерывания и причина перезагрузки не установлены;
mtime результата не выдаётся за время перезагрузки. После сообщения пользователя
выполнены только проверка и сохранение оставшегося скрипта, контроль целостности
и оформление отчёта. Плановый срок 07:30 сохранён выше; отчёт закрыт позднее.
Время закрытия: **07:57:51 МСК / 04:57:51 UTC**.

**Завершены 17 проверенных блоков; все ядра tools ещё не готовы.**
Это счётчик выполненных блоков текущей миграции, не процент готовности классов.
Прежние 16/16 операций другого измерителя не означают завершение новой задачи.

| Направление | Внедрено в общий восстановленный код и подключено к инструментам |
| --- | --- |
| Чтение SMO | FFPS/FAT/заголовки объектов, префиксы ссылок, поля Node и CollisionInfo, render mesh, MeshBV/wxFaceData, raw/PC texture sections; PS2 mesh header/bounds |
| Поза и анимация | Исходные Node world/PRS и SAN sampler/controller; SanToVmd использует общий SMO/SAN graph и расчёт позы |
| Запись | BGRA texture/mips, embedded MemoryStream, поддерживаемые заголовки полей, общий MeshData writer вместо трёх C# реализаций, MeshBV writer для добавления коллизий |
| Производительность | Кэш индекса объектов: выделения managed-памяти на проходе 835 узлов снижены с 204,4 до примерно 1,7 МБ (99,17%); это отдельная операция, не FPS |

Проверки проводились на подходящих небольших выборках с прямыми original-PC
сравнениями. Последний блок: 5 точных original states/matrices, C++54,
5 Viewer samples (включая Alfea02), 3 collision append; сборки Viewer/Importer
без предупреждений и ошибок. При закрытии совпали все 27 файлов
последнего evidence snapshot и native DLL. Полные тесты без изменения кода
повторно не запускались. Проверочная сборка WPF ранее прошла; выпуск и проверка
всего интерфейса в этом цикле не выполнялись.

**Осталось:** части skin/material/texture wrapper и PS2 texture logic,
StaticRenderObject/Zone/PartitionSystem для полного графа уровней,
runtime-владелец и регистрация коллизий с окончательным world placement,
оставшаяся запись/редактирование графа и служебные игровые алгоритмы Viewer.
Ограничения приложений не скрыты успешными заглушками.

### Случаи, оставленные на отдельное решение

1. **Packed MeshCombiner:** оригинальный cached byteSize расходится с физическим
   объёмом packed vertices (120 против 84 байт на примере). Standalone mesh
   путь проверен; точное представление combined cached member отложено.
   Рекомендация: отдельно представить оригинальное поле и физическое хранилище
   после подтверждения затронутой ветки; не исправлять игру ради размера буфера.
2. **Редкие заголовки полей при редактировании:** исходный writer не выражает
   некоторые нужные lossless формы (реальное пустое поле, ID31, принудительный
   extended low ID). Эти формы явно отклоняются. Предложен один общий редакторский
   адаптер после согласования, с сохранением исходного writer как эталона.
3. **Запись мировой позы при неравномерном масштабе родителя:** прежний inverse
   editor способен выдать local PRS с другим игровым world. Общий forward расчёт
   выявляет это и отказывает до выдачи результата. Рекомендация: сначала найти
   оригинальный world-to-local путь; при его отсутствии предложить общий
   редакторский адаптер и проверить его исходным forward расчётом.

Решённые по ходу случаи: исправлены стенды startup/ownership и ложные ожидания
тестов; установлен consuming-контракт MemoryStream; исправлены XRGB alpha,
самовольная нормализация quaternion/normal и выдуманная короткая nonnull ссылка;
замена inverse-bind world общим Node runtime проверена по оригиналу.
Ограничения приёма unsupported/NaN SAN и inspector class resolution сохранены
явно. Подробные условия каждого исправления находятся в досье блоков ниже.

### Последний незавершённый исследовательский шаг

Сохранён `research/probe_pc_static_matrices.py`: исходные factories 41A7C0/44FCE0,
Matrix4 startup 6D38C0, размеры 268/20 байт, обе начальные матрицы identity,
освобождение обоих оригинальных владельцев. Для teardown явно предоставлено
нулевое renderer cache storage 0xCA00 внутри micro arena; renderer constructor,
device и графические вызовы не исполняются. Первый прежний опыт без этого
хранилища остановился на unmapped C190; это неполный setup, не дефект класса.

При закрытии уточнена только выдача трёх slots secondary interface; четыре
соседних слова primary vtable больше не помечаются как её slots. Новый свежий
ограниченный запуск прошёл, остальные результаты точно равны прежнему логу.
Это constructor/teardown scout, **не внедрённый serializer или блок18**.
Чтение/запись StaticRenderObject и его подключение к ядру остаются следующей
работой. Логи локальные, игровые файлы и сборки в Git не добавляются.

Основной checkpoint `3b23bc1`, Viewer `4a407fd`, TextureTool `1ddb50f`.
Итоговый снимок: `research/tools-core-cycle-stop-2026-09-09.json`.
GitHub push остаётся приостановленным: ранее автоматическая проверка одобрения
отклонила публичную отправку; запрос подтверждения конкретных репозиториев
остаётся без ответа. Основание и адреса сохранены в
[предыдущем отчёте](tool-cycle-report-2026-09-08-1900.md).

## Порядок

1. Общие препятствия загрузки/сохранения native SMO graph: `spCollisionInfo`,
   нужные bounding-volume владельцы и их связи с Node/Partition.
2. Общий доступ приложений к реальным mesh/skin/material/texture ресурсам;
   замена оставшегося самостоятельного разбора на C++.
3. Общие операции записи и редактирования для Importer/LVLcreator/TextureTool.
4. SAN-конвертер и оставшиеся специфические пути приложений, с отделением
   целевых форматов и собственного платформенного кода от игровой семантики.
5. Адресные проверки операций ядер, честный учёт незавершённого и UI-поломок.

Предыдущий аудит Viewer: `e06f46a`, submodules Viewer `c909e4d`, TextureTool
`909ab20`. Новое поручение разрешает исследовать прежние пробелы V-03/V-04/V-06
и менять соответствующие ядра; необходимости ждать решения об интерфейсе нет.
Правило предложения альтернативной реализации оригинального алгоритма сохраняется.

## Границы и учёт

Реконструкция использует PC как основной источник и PS2 для полезных сравнений.
Unknown branches не получают успешных заглушек. Существующее поведение инструмента
не считается эталоном. Наборы проверок выбираются по различиям, обычно 5–15
реальных файлов плюс необходимые синтетические границы. Память около 1 ГиБ
суммарно; компиляция по два процесса, другие профили — согласно замерам.
Проверенные блоки сохраняются локальными commits; release не упаковывается.

Готовность каждой операции: подтверждённый нужный контракт → единственная
внедрённая реализация → проверка результата. Наличие одного восстановленного
класса или успешная сборка не закрывают всю операцию.

## Результаты и вопросы

### Первый блок: CollisionInfo/OBB

Восстановлены классы и реальные reader/writer/ownership/transform contracts;
Node и его serializers подключены к ним. Дубликаты обхода Node relationships
в RenderNode/Light/LightData сведены к одному методу. Восемь original-PC micro
прогонов и отдельная загрузка C++-written полного графа прошли. 31/31 native
allocation освобождены; C++ 55 checks и шесть выбранных suites прошли.

Подробности и явные границы:
`docs/research/native-pc-collision-tools-core-2026-09-08.md`. Query/contacts/
broadphase не входят в нужное инструментам API. Наличие этого блока ещё не
закрывает native загрузку произвольного SMO или миграцию любого приложения.

Порядок следующего шага уточнён ради скорости: SAN-конвертер уже может получить
готовый общий native sampler, без ожидания spMeshBV. После этой самостоятельной
миграции продолжается общий SMO resource graph и доступ к его данным.

Нестандартные случаи не маскируются. Два диагностических сбоя первого стенда
оказались недостающим startup clone-map и неверным ожиданием OBB world position;
исправлены fixture/ожидания по оригиналу. Первый C++ graph test требовал явного
save-dispatch в своём setup; настройки исходного serializer manager не менялись.

### Второй блок: SAN и мировая поза SanToVmd

Самостоятельный Python sampler/decoder SAN и игровой FK удалены. Приложение
использует общее C++ animation/node ядро через ctypes, с одним вызовом мировой
позы на кадр и повторным использованием сцены. 32 коротких теста, 24 VMD / 55 024
ключа на трёх PMD, 732 original-PC-reference позы и 2 273 C# interop checks
прошли. Шесть C++ suites прошли. SMO-reader скелета ещё прежний: весь SanToVmd
не отмечен готовым. Следующий блок — общий SMO resource graph.

Отличия приёма входов: общий loader отклоняет unsupported representation даже
в ненужном треке и NaN в сырых коэффициентах. Прежняя возможность пропуска не
восстановлена обходным parser; граница задокументирована. Подробности:
`docs/research/tool-san-shared-core-2026-09-08.md`.

### Третий блок: общий SMO graph

SanToVmd полностью подключён к общей реконструкции игровой части: SMO/SAN
readers и собственный игровой FK удалены. Сцена получает реальные загруженные
узлы с исходными матрицами. 37 tests, 24 VMD / 732 native-reference позы,
пять выбранных character SMO, шесть C++ suites и 2 273 C# checks прошли.
Подробности: `docs/research/tool-resource-graph-core-2026-09-08.md`.

Остальные приложения ещё требуют подключения к native данным ресурсов.
Следующий конкретный пробел: MeshBV у меню/уровней. В процессе сборки обнаружено
обновление VS с pending reboot; явный путь к имеющемуся toolchain позволил
продолжить без вмешательства в установку и без перезагрузки.

### Четвёртый блок: MeshBV и игровые face data

Общий core получил spMeshBV/spCollisionMesh/spFaceDataContainer и wxFaceData
с настоящей RTTI-цепочкой. Геометрия пользуется существующими index/vertex
buffers; sphere producer общий с render mesh, без второй реализации.
Пять original-PC micro cases прошли, все 2/10/14/14/18 allocations освобождены.
Sphere совпал побитно с оригиналом на семи отдельных случаях. C++76 checks
и семь выбранных suites прошли. Меню1 225 objects, SFX tile_bad127 и Bloom121
прошли полный общий loader. ParticleSystem потребовал лишь регистрации уже
восстановленного reader. Query-tree/collision queries не входят в этот срез.
Подробности: `docs/research/native-pc-mesh-bv-tools-core-2026-09-09.md`.

Следующий блок — убрать самостоятельный C# разбор контейнера и data blocks:
использовать восстановленные чтение header/FAT/fields, сохраняя диагностику
и собственные сценарии инспектора. Существующий C# typed resource decode
тоже требует последующей замены, готовность всех ядер пока не заявлена.

### Пятый блок: контейнер/FAT в общих C# cores

Самостоятельный FFPS/FAT/object-header parser из SmoDocument удалён. Используются
общие исходные операции чтения; runtime RTTI rejection сохраняет прежний порядок,
а raw inspector получает metadata без фиктивных объектов неизвестных классов.
SafeHandle живёт только при batch передаче metadata, исходный SMO не копируется
в native stream. Осталась собственная диагностика и ещё не перенесённые typed
resource decoders. Введён адресный выбор C++ suites через CheckSuites.

Сборки прошли; семь прежних C++ suites и FullLoader213 checks прошли. C# FormatTests
на одном PC меню:9 262 assertions. Native metadata сверены на пяти файлах,
включая два с настоящими PS2 payloads; полные resource graphs на меню/SFX/Bloom
сохранили результат. Досье: `docs/research/tool-container-shared-core-2026-09-09.md`.
Отдельный финальный C# прогон PS2-меню прошёл2 937 assertions.

### Шестой блок: MeshBV reader в приложениях

C# геометрия MeshBV и wxFaceData теперь используют общие восстановленные
классы через leaf ABI, без самостоятельных byte parsers. Native stream сообщает
смещение вершин редактору; повторное чтение ради смещения удалено. Приложение
теперь принимает native unknown/repeated face fields и независимый face count.
83 C++ checks, C#9 271/2 946/1 504 на PC menu/PS2 menu/tile_bad прошли;
два полных resource graphs сохранили результат. Досье:
`docs/research/tool-mesh-bv-shared-core-2026-09-09.md`.
Следующий приоритет — общий render mesh reader и его typed buffer view.

### Седьмой блок: render mesh и vertex layout

C# E0/E1/PC metadata parsers и собственная таблица offsets заменены общими
классами. Whole reader выбирает original поле по платформе; normal сохраняет
значение файла. Пять свежих original-PC/tools ABI сравнений, FullLoader213,
C#9 282/2 957/1 505 на PC menu/PS2 menu/Bloom и два whole graphs прошли.
PS2 native packet metadata пока требует переноса. Досье:
`docs/research/tool-render-mesh-shared-core-2026-09-09.md`.

Отдельно сообщено прежнее расхождение packed combiner cached byteSize. Новая
standalone mesh загрузка совпала с оригиналом; выдача комбинированного cached
member за точное native состояние приостановлена, рекомендация разделения
игрового member и host storage size записана в досье.

### Восьмой блок: PS2 mesh metadata

PS2 header/bounds byte parser перенесён из C# в spPS2MeshDataSerializer.
Два original-PC prefix observations до packet allocation и полный bounds-only
field reader прошли, все3/3,3/3,5/5 allocations освобождены; PS2 операции
сопоставлены статически. ABI сохранил40/40/24 bytes. C#2 960/9 833/9 285
на двух PS2 меню и одном PC меню прошли. Убраны вымышленные ограничения
counter/format и запрет reverse bounds; DMA runtime не реализуется.
Оставшаяся C# metadata aggregation и другие resource readers ещё в работе.
Досье: `docs/research/tool-ps2-mesh-metadata-core-2026-09-09.md`.

### Девятый блок: raw/PC texture sections

Raw pixels и PC mip records переданы общим serializers; C# получает slices
исходного SMO. TextureTool также больше не читает FFPS header самостоятельно.
Исправлено смешение XRGB и BGRA: общий codec готовит opaque alpha для preview
и PNG, не меняя raw bytes. Семь свежих original/ABI сравнений, C++1007,
C#9300/2975 и TextureTool4248+40 прошли. Source-wrapper/PS2 metadata и writers
ещё требуют переноса. Досье:
`docs/research/tool-texture-sections-shared-core-2026-09-09.md`.

### Десятый блок: общий texture writer

Создание BGRA leaf и замена mip payload переданы исходным C++ writers.
Выяснено настоящее consuming ownership embedded MemoryStream; прежний вывод
о восстановлении cursor исправлен новым досье. Generic dangling attachment
не экспортируется в host. Четыре original writers, два source cases, точный
ABI, C++1007, TextureTool4272 и Importer192 прошли. Все32 edited outputs
побайтно прежние; generic editing/field encoder остаются следующим переносом.
Досье: `docs/research/tool-texture-shared-writer-2026-09-09.md`.

### Одиннадцатый блок: общий field header writer

Удалён C# encoder заголовков полей; общая запись через шестибайтный borrowed
stream.14 свежих original/ABI headers совпали, C++340 и C#9300/2975 прошли.
Тестовый выбор поля уточнён из-за настоящего канонического terminator00.
Оригинал не пишет size0 и неправильно выдаёт ID31; эти формы, а также forced
extended low ID приостановлены с явной ошибкой. Предложен отдельный общий
lossless-адаптер после решения пользователя. Досье:
`docs/research/tool-field-header-shared-writer-2026-09-09.md`.

### Двенадцатый блок: общие reference prefixes

Node/material helpers удалили собственную reference grammar. Общий resolver,
его bounded guards и inspector используют один prefix reader. Убрана выдуманная
legacy short nonnull ссылка; тестовые Node/Model исправлены, BSP Zone проверяет
подтверждённый null. Три original пути, split stream, failed-size stop, пять
ABI guards, C++266, четыре C# набора и три whole graphs прошли. Досье:
`docs/research/tool-reference-prefix-shared-core-2026-09-09.md`.

### Тринадцатый блок: Node scalars и transform adapter

Общий scalar reader вместо C# PRS/flag parser, quaternion без нормализации,
явное разделение authored/effective flags. Запасное угадывание transform удалено.
Пять свежих original cases совпали побитно; C++226, четыре C# набора и три
whole graphs прошли. Кэш host ID index снизил managed allocations на проходе
835 nodes с204.4 до1.7 МБ; это отдельный замер, не FPS. World placement policy
ещё требует переноса. Досье: `docs/research/tool-node-scalars-shared-core-2026-09-09.md`.

### Четырнадцатый блок: world matrices

Удалены C# FK и inverse-bind подстановка из placement helpers; общий scene
runtime даёт исходный Node world. Свежий original parent/child case и1054
world matrices whole graphs совпали побитно. C++86, пять Viewer наборов,
Alfea02 с132 collision meshes и WPF build прошли. Старый inverse editor при
неравномерном parent scale даёт неверный world: финальная native проверка
теперь явно отказывает; эта ветка приостановлена, предложен исследованный
original inverse path либо согласованный общий editor adapter. Досье:
`docs/research/tool-node-world-shared-core-2026-09-09.md`.

### Пятнадцатый блок: общая запись мешей

Три C# writer в импортёре/level workflow (resource replacement, split skinned
chunks, skeleton carrier) используют один native MeshData writer. Original
IB/VB/mesh owners и serializers без изменений; host DTO без wire grammar.
Девять original streams, пять host refusals, C++346, девять замен91 check,
две clean-skinned пересборки26 checks. Все11 полных outputs побайтно прежние.
Досье: `docs/research/tool-mesh-shared-writer-2026-09-09.md`.

### Шестнадцатый блок: запись collision MeshBV

Level appender использует исходный MeshBV writer и owning CPU classes. Два
original cases/три guards, C++83 и три appended branches Alfea01/02/03 прошли;
полные outputs побайтно прежние. Общая input preparation повторно проверена
девятью render-mesh original cases. Исходные template/geometry ограничения
appender сохранены. Досье: `docs/research/tool-collision-shared-writer-2026-09-09.md`.

### Семнадцатый блок: CollisionInfo scalars

Общие field1/2 bodies вместо C# numeric reader/normalization; preserved raw Q,
native defaults, repeated/unknown fields. Матрица stored PRS — общий affine.
Пять original states/matrices точны; C++54, пять C# samples и три collision
append прошли. Runtime owner/registration и финальный collision placement
отделены от stored PRS и остаются задачей. Досье:
`docs/research/tool-collision-scalars-shared-core-2026-09-09.md`.
