# Срез переноса ядер tools — 9 сентября2026

Составлен при закрытии цикла до19:00 МСК. За этот цикл завершены22 проверенных
блока. Это число отдельных изменений, не22 готовых класса и не процент
завершения всех ядер. Исторические16/16 операций оценивали прежний набор
функций; они не подтверждают соответствие всей новой архитектуры.

## Практическая готовность

| Потребитель | Подключено к общему восстановленному коду | Что мешает завершить ядро |
| --- | --- | --- |
| Viewer | Loaded SMO graph, actual Node/Model/Skin/material/texture references, все render support slots; SAN; общий material runtime API; navigation, LensFlare и Particle snapshots; часть spatial/Fog inspection | Остаточные C# inspectors; MaterialColor factory, Occlusion full Init, source-less texture case; подключение material clock и специальных GPU passes |
| Exporter | Поза/parents из actual Node; общие geometry, Model variants и все placements в GLB/FBX/OBJ; общий SAN | Полная проекция material passes/layers в целевые форматы; оставшийся аудит специальных ресурсов и authoring/sampling adapters |
| Importer | Общие mesh/texture/skin writers из прежнего цикла; actual placements и материалы SMO donor | DTO не выражает multipass/layers; полный путь сборки/записи графа и оставшиеся authoring правила ещё требуют переноса/проверки |
| LVLcreator | Workspace сохраняет все actual support slots; общий scene/mesh/collision/pose код | Команды не адресуют повторные slots; world-to-local при nonuniform parent; полный граф записи и collision/Scene attachment |
| TextureTool | Общие texture sections/codecs/writers предыдущего цикла, использует общий Core | Остаточные правила SmoTextureDataDecoder, PS2/старые source формы и адресная проверка всего ядра; этот цикл не добавляет ему отдельного полного acceptance |
| SanToVmd | Общий SMO/SAN graph и native pose sampler из предыдущего цикла | Новый полный acceptance конвертера в этом цикле не проводился; VMD/PMD conversion остаётся прикладным адаптером |
| WinxHairPatcher | Отдельный инструмент сигнатурной правки EXE; его файловый IO и patch bytes являются прикладной операцией | В этом цикле не менялся и не проверялся целиком по новым правилам. Это не альтернативный SMO loader или игровая runtime class |

`FbxBridge.Native` и `SparkplugViewer.Native` — общие мосты, а не отдельные
игровые реализации. UI layout и выпуск во всех строках вне текущего этапа.

## Очередь по конкретным остаткам

1. **Убрать оставшиеся самостоятельные spatial inspectors**, используя уже
   существующие actual classes/readers: `SmoPartitionNodeDecoder`,
   `SmoBspNodeDecoder`, `SmoPartitionRenderableDecoder`, `SmoZonePortalDecoder`,
   соседние Zone/PortalNode/System projections. Octree own fields уже native,
   inherited metadata всё ещё проходит через старый PartitionNode decoder.
   Не переносить дополнительные C# ограничения канонического порядка/plane
   normalization в восстановленные классы.
2. **Light и простые bounding volumes:** `SmoLightDataDecoder`,
   `SmoSphereBoundingVolumeDecoder`, `SmoBoxBoundingVolumeDecoder`,
   `SmoOrientedBoxBoundingVolumeDecoder`. Проверить необходимые общие read
   методы и заменить payload/default logic, как сделано для Fog.
   Для Light общий `ReadLightFields` уже находится в
   `Sparkplug/Code/Sparkplug/spLightSerializer.cpp` и обслуживает LightData/DXLight.
   Он допускает повторные/unknown fields и nonzero flag bytes, которые старый
   C# decoder ограничивает. Цвет actual Light хранится как RGBA floats:
   не выдавать обратную упаковку цвета за неизменённый authored UInt32.
3. **Texture metadata:** `SmoTextureDataDecoder` всё ещё задаёт собственные
   ограничения top-level/source формы; `SmoTextureDecoder` хранит legacy
   header inspection. Отличать сырые сведения от actual loaded pixels.
4. **Ресурсы с недостающей логикой:** `SmoTextDecoders` (Font/TextRenderable),
   `SmoOcclusionVolumeDecoder` и actual full Init, PC MaterialColor constructor,
   looping particle initialization. Исследовать только нужные потребителям
   методы. PS2 constructor/color startup записаны отдельно, PC factory NULL.
5. **Запись и runtime:** оставшаяся сборка графа Importer/LVLcreator,
   authoring adapters и special frame/render passes. Совпадение имён методов
   само по себе не доказывает дубль; целевые форматы и собственный OS/GPU
   backend остаются разрешённым кодом приложения.

Это адресный остаточный аудит исходников, а не полный корпусный прогон и не
утверждение, что перечисление исчерпывает все возможные дефекты. Следующий
цикл начинает с первого необходимого потребителя, используя малую выборку
из базы и прежние original captures по неизменённым входам.

## Явно отложенные случаи

- По решению пользователя до работы с LVLcreator: cached120/physical84,
  редкие lossless headers и inverse world при nonuniform parent.
- Повторные support slots: загрузка/Workspace сохраняют их, LVL commands
  отклоняют `REPEATED_RENDERABLE_AUTHORING`. Нужна согласованная адресация слота.
- Multipass donor: импортный DTO отклоняет `MATERIAL_IMPORT_SHAPE`.
  Предложение — расширить общий imported material model без потери passes/layers.
- Повторная непустая NavigationGraph table: оригинал даёт повторное освобождение;
  host явно отклоняет ввод. Нужна отдельная политика редактирования такого случая.

Ни один из этих отказов не заменён успешной заглушкой. Правила игры не
подправлялись ради ожидаемого результата приложения.

Основные доказательства и покомпонентные ограничения:
[отчёт цикла](tools-core-cycle-2026-09-09-1900.md),
[PS2 MaterialColor](material-color-ps2-constructor-2026-09-09.md).
