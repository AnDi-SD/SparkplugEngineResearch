# Общий SMO resource graph и завершение игровой части SanToVmd

Этап цикла до 07:30 МСК. `ResourceGraph` — прикладной владелец результатов
настоящего `spSerializerManager::LoadResourcesForAnalysis`. Он регистрирует
готовые readers Node/RenderNode/Model/Skin/MeshData/MaterialData/TextureData/
LightData/Fog/CollisionInfo/OBB и material controllers. Самостоятельного
декодирования полей или геометрии в мосте нет.

## Владение и платформа

Serializer manager и resource cache существуют только во время загрузки файла.
После успешной загрузки host удерживает shared owners созданных объектов,
наблюдённые FAT identities и владельца CPU declarations. Контекст чтения
не сохраняется с висячими ссылками на manager/cache. Разные документы не
пользуются одним кешем ресурсов по имени.

Единственный `spPCRenderer` используется как CPU-владелец описаний вершин.
Он не получает устройство, не запускает DirectX/COM и не выполняет старый
platform startup. Mesh/texture serializers заполняют уже восстановленные
CPU buffers/shadows. OpenGL/Vulkan backend остаётся отдельным кодом приложений.
Сохранение runtime имён классов DXMesh/DXTexture/DXMaterial не означает вызов
DirectX API; именно эти фабрики подтверждены оригинальным PC-кодом.

Необязательный host observer FAT дополнен wire class ID, offset, size и именем.
Он копирует готовые данные перед штатной очисткой FAT. Reader, dispatch и
исходные игровые алгоритмы не изменялись. Wire class и runtime class намеренно
различаются там, где оригинальная фабрика создаёт платформенный объект.
`rootID` API — первый ресурс, возвращённый generic loader; действительные
родители узлов берутся из `spNode`, а не из позиции записи в файле.

## SanToVmd

Удалены оставшиеся Python `read_ffps`, `fields`, самостоятельный SMO reader
и нормализация локального quaternion игрового узла. `Skeleton` хранит имена,
метаданные MMD-профиля и native graph. `spv_graph_scene` выбирает реальные
загруженные `spNode`, требует всех их родителей и удерживает весь граф.
Чтение parent/child охватывает производные Node, включая RenderNode и Light.

Сцена сохраняет исходную матрицу, не делает цепочку matrix → quaternion →
matrix для повторного создания узла. Synthetic nonunit quaternion `(0,0,.5,.5)`
даёт исходную позицию ребёнка `(.5,.5,0)`, а не результат нормализации.
Тот же Scene runtime обслуживает прежний C# API и новый graph-backed API.
Сцены над одним graph используют одни изменяемые объекты; отдельные документы
имеют отдельные узлы. Интерфейс не обещает независимые копии одного graph.

PMD/VMD, retargeting, выбор имён и ограничение масштаба остаются собственными
операциями конвертера. Для проверенного набора персонажей игровая часть ядра
полностью использует общую реконструкцию. Это не завершение всех tools.

## Проверка

- 37 коротких проверок, включая matrix preservation, производных родителей,
  удержание графа сценой после закрытия Python handle и независимость документов.
- 24 VMD / 55 024 ключа; 732 original-PC-reference позы, независимый VMD reader.
  Max direction error `1.744849e-6`; последний выбранный прогон 8.894 секунды.
- Bloom 121/98, Icy 119/88, Knut 108/88, Flora 108/83, Tecna 104/82
  объектов/узлов. Все FAT identities/names/extents совпали с кешированным
  индексом тех же байтов (SHA256), parent/child counts согласованы.
  Пять загрузок заняли по 0.006–0.012 секунды в прогоне валидатора.
- Шесть C++ suites прошли после сборки. Прежний C# interop: 2 273 проверки,
  0.993 секунды, peak working set 49 463 296 байт.

Воспроизведение: `research/validate_tools_resource_graph.py --output
local-data/results/tools-core-cycle-20260909-0730/resource-graph/report.json`,
обычные тесты SanToVmd и его `validate_rare_local.py`. Набор/зависимости команды
VMD приведены в README; последний output расположен в `san-native-graph-vmd-final`.
Snapshot: `research/tools-core-resource-graph-block-2026-09-08.json`.

FAT equality — проверка интеграции с прежним индексом, не новый запуск игры.
Original-PC PRS взят из проверенного frozen reference, с повторной проверкой
его входов. Полного corpus и новой визуальной MMD/GPU проверки не было.

## Следующие пробелы и сборочное окружение

`igmenu_opt_pc.smo` пока отклонён: 33 MeshBV ещё не имеют общего runtime
владельца/reader. Они не заменены заглушками. Это следующий адресный приоритет
для меню и уровней. Поля mesh geometry, material/texture и editing ещё нужно
подключить к C#-потребителям; наличие native graph само их миграцию не закрывает.

Во время цикла Visual Studio обновилась и стала `isComplete=false`,
`isRebootRequired=true`; стандартный vswhere её больше не выбирал.
Добавлен явный `-VisualStudioPath` в Build-Native, проверяющий наличие toolchain.
Установленный компилятор успешно собрал DLL без перезапуска/изменения системы.
При пересборке прежнего spDXMesh компилятор выдал C4756 в bounds-коде; этот
исходник не менялся, проверки прошли. Предупреждение не скрыто и не исправлено
подгонкой восстановленной математики. Release не упаковывался.
