# Цикл ядер tools до 07:30 МСК, 9 сентября 2026

Начат 8 сентября около 22:15 МСК. Срок отчёта: **2026-09-09 07:30 Europe/Moscow**
(`2026-09-09T04:30:00Z`). Статус: в работе.

Цель работы — ядра всех приложений `tools/` по новым правилам. Совместимость UI
учитывается отдельно; серьёзная переделка UI не задерживает ядро. Исследование
сначала закрывает конкретные недостающие методы/поля используемых классов.
Если ядра готовы раньше срока, цикл продолжается последовательным разбором EXE.

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
