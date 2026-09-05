# Восстановление исходной архитектуры Sparkplug

Статус: начальная граница доказана полными PC- и PS2-registration graph.
Разбиение на исходные проекты подтверждено только там,
где в PC executable сохранился путь исходного файла. Названия аналитических
групп ниже не выдаются за утраченные имена проектов.

## Цель и правило именования

Новая цель — не написать похожий по поведению движок с удобной собственной
архитектурой, а постепенно восстановить наблюдаемый Sparkplug с исходными
именами типов, отношениями наследования, границами платформенных слоёв и порядком
работы подсистем. Winx Club рассматривается как отдельный клиент движка.

Для слоя реконструкции действуют жёсткие правила:

1. Имя класса или подсистемы используется только после обнаружения строки,
   registration, PDB/source path либо другого прямого бинарного evidence.
2. Функция без доказанного имени остаётся `sub_<VA>` или `sub_<RVA>`.
   Пояснение её роли хранится отдельным аналитическим alias и не становится
   именем API.
3. Папки и проекты повторяют только подтверждённые имена исходных модулей.
   Новая папка с предполагаемым оригинальным именем не создаётся заранее.
4. Префиксы `sp` и `wx` — сильный первичный признак, но не единственное
   доказательство. Решение проверяется наследованием, source path, call graph и
   сравнением PC/PS2.
5. Современные классы наших Viewer/Importer/LVLcreator не считаются моделью
   исходного движка и не переносят свои имена в реконструкцию.

## Первая доказанная граница

Для PC-файла с SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`
разобраны все 733 прямых вызова конструктора registration по RVA `0x0012FF0`.
В PS2 ELF с SHA-256
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`
разобраны все 681 MIPS-вызова того же механизма по VA `0x001115B0`, включая
короткие tail-thunks и три регистрации с промежуточной инициализацией таблиц.
Каждая запись даёт строку имени, class hash, base class hash, указатель на base
registration, адрес собственного registration object и два пока не названных
callback-аргумента.

| Evidence | Engine `sp...` | Winx `wx...` | Всего |
|---|---:|---:|---:|
| PC registrations | 329 | 404 | 733 |
| PS2 registrations | 275 | 406 | 681 |
| Совпадают между PC и PS2 | 231 | 399 | 630 |
| Только PC | 98 | 5 | 103 |
| Только PS2 | 44 | 7 | 51 |

Все 681 строгие class-like строки PS2 сопоставлены с конкретными регистрациями.
У всех 630 типов, общих для PC и PS2, совпадают class hash и base class hash:
расхождений идентичности или непосредственного наследования нет. Типы только
одной платформы всё ещё означают различие конкретных сборок, а не автоматически
принадлежность всего семейства исключительно одной платформе.

Найдены 42 прямых ребра из игрового слоя в движок на PC и 44 на PS2. Наиболее
показательные общие ребра:

| Игровой тип | Базовый тип движка | Class ID |
|---|---|---:|
| `wxEngineCore` | `spEngineCore` | `0x34B85918` |
| `wxPCApp` | `spPCApp` | `0x707D09F3` |
| `wxEntity` | `spEntity` | `0x796A1869` |
| `wxButton` | `spButton` | `0x69F878BA` |
| `wxFaceData` | `spCustomAppData` | `0x313C4C17` |
| `wxGameTimer` | `spTaskTimer` | `0x78665312` |

Это показывает исходную схему расширения: Winx-типы не образуют самостоятельный
движок, а наследуют и используют общие Sparkplug-типы. Обратных ребёр `sp -> wx`
нет ни в PC-, ни в PS2-registration graph.

## Подтверждённые исходные модули

В PC executable сохранились 53 уникальных пути под
`Z:\Sparkplug\Code\...`: 52 относятся к коду движка, один — к вложенному
`Sparkplug\External\Ftsg`. Они доказывают следующие исходные имена модулей:

| Исходный модуль | Уникальных путей | Прямые примеры состава |
|---|---:|---|
| `SparkBase` | 6 | `spBaseObject.cpp`, `spNew.cpp`, `spErrorManager.cpp`, `spMemoryStream.cpp`, `spSocketStream.cpp`, `spSTL_allocator.h` |
| `SparkBasePC` | 1 | `spPCFileStream.cpp` |
| `Sparkplug` | 31 | `spEngineCore.cpp`, serializer classes, `spAudioManager.cpp`, `spGUIManager.cpp`, `spTemplate*.cpp` |
| `SparkplugDX` | 7 | `spDXRenderer_Init.cpp`, `spDXMesh.cpp`, `spDxTexture.cpp`, render targets и shared mesh data |
| `SparkplugPC` | 7 | `spPCApp.cpp`, font/shader/effect loaders, `spDXAudioBankEntry.cpp` |
| `Sparkplug\External\Ftsg` | 1 | `memory.c`; вложенный сторонний код, не собственная подсистема Sparkplug |

`spDXMeshDataSerializer.cpp`, `spPS2MeshDataSerializer.cpp` и соответствующие
texture serializers лежали именно в исходном модуле `Sparkplug`. Значит, одно
лишь `DX`/`PS2` в имени класса или файла не позволяет переносить его в другой
проект.

Для PS2 видны точные семейства классов `spPS2...` и `wxPS2...`, но имя исходного
PS2-модуля или проекта пока не найдено. Создавать вымышленный `SparkplugPS2`
как якобы доказанное исходное имя нельзя.

## Первичная нарезка подсистем

Названия в первом столбце — только навигационные аналитические группы. Имена во
втором столбце присутствуют в registration/source evidence буквально.

| Аналитическая группа | Доказанные исходные типы и файлы | Что нужно установить дальше |
|---|---|---|
| Object/RTTI/core | `spBaseObject`, `spNamedObject`, `spCrossPlatform`, `spRTTIManager`, `spEngineCore`, `wxEngineCore` | layout registration, factory callbacks, init/shutdown order |
| Stream/resource | `spStream`, `spMemoryStream`, `spFileStream`, `spPCFileStream`, `spPCKManager`, `spResource`, `spResourceManager` | raw/PCK resolver, cache и lifetime |
| Serialization | `spSerializer`, `spSerializerManager`, `spSerializerHook`, `spDXSerializerHook`, `spPS2SerializerHook`, `spDataBlockSerializer.cpp`, `spResourceFATSerializer.cpp`, многочисленные `*Serializer` | имя/slot platform hook, identity FAT helper, save/fixup passes |
| Scene graph | `spScene`, `spSceneManager`, `spNode`, `spRenderNode`, `spRenderable`, `spModel`, `spSkin` | ownership, update и traversal |
| Renderer/resources | `spRenderer`, `spDXRenderer`, `spPCRenderer`, `spMesh`, `spTexture`, `spMaterial`, buffer/target/shader classes | interface slots и создание GPU resources |
| Spatial/visibility | `spPartitionSystem`, `spPartitionNode`, `spBSPNode`, `spOctreeNode`, `spZone`, `spZonePortal`, `spOcclusionVolume`, `spVisibilityManager` | culling order и scene registration |
| Collision/physics | `spCollisionInfo`, `spCollisionMesh`, `spBoundingVolume`, `spConstraintSystem`, `spRigidBody`, `spCollisionManager`, `spPhysicsManager` | разделить сериализуемые данные и runtime simulation |
| Animation/controller | `spAnimation`, `spAnimationManager`, `spController`, `spRenderController`, `spTrack`, evaluator classes | update graph, binding и pose consumers |
| GUI/font | `spGUIManager`, `spGUIObject`, `spWidget`, `spButton`, `spTextNode`, `spTextRenderable`, `spFont` | input/focus/layout/render path |
| Audio | `spAudioManager`, `spAudioBank`, `spAudioVoice`, `spAudioListener`, `spAudioSound` и PC/PS2 derivatives | bank loading, voice lifetime и platform backend |
| App/input/platform | `spApp`, `spPCApp`, `spInputManager`, `spInputDevice`, `wxPCApp`, PC/PS2 derivatives | bootstrap и граница engine callback/game callback |
| Engine templates/levels | `spTemplate`, `spTemplateObject`, `spTemplateManager`, `spGameLevel`, `spGameLevelSerializer` | определить generic contract до анализа Winx SPT/SPL |
| Winx gameplay | `wxEntity`, `wxCharacter`, `wxGameFlowController`, `wxGameFlowState`, `wxCharacterRegistry`, `wxAssetManager` и остальные `wx...` | разложить game layer по его собственным managers и state machines |

Слово `Game` в `spGameLevel` или `spGameCore` не делает тип Winx-специфичным:
его registration находится в `sp`-иерархии. Аналогично `spPC...`, `spDX...` и
`spPS2...` — платформенные части движка, тогда как `wxPC...`/`wxPS2...` остаются
платформенными частями игры.

## Как это меняет трактовку файлов

Граница проходит не по расширению целиком. Например, `FFPS`, FAT и общий
serializer dispatch относятся к движку, но тот же контейнер способен создать
зарегистрированный application type. `wxFaceData -> spCustomAppData` — уже
доказанный пример такой точки расширения. Поэтому база ресурсов должна хранить
отдельно:

- формат и transport, которыми владеет Sparkplug;
- class registration и его owner boundary;
- конкретный serialized object;
- game-specific consumer или producer;
- platform implementation.

Текущая таблица SMO `classes` остаётся реестром классов, встреченных или важных
для форматов. Она не должна незаметно превращаться в полный runtime type catalog.
Для полного каталога нужны отдельные сущности `native module`, `native type`,
`native inheritance edge`, `native function` и `ownership evidence`, связанные с
конкретным executable hash. До добавления этих таблиц воспроизводимым источником
служит scanner ниже.

## Воспроизведение

Краткая сводка PC, PS2 и их пересечения:

```powershell
python -B research\inspect_executable_architecture.py `
  local-data\pc-pristine\WinxClub.exe `
  "local-data\Winx Club the game PS2\SLES_532.19"
```

Полный список exact names и source paths можно вывести ключами `--show-types` и
`--show-sources`, а машиночитаемый результат — ключом `--json`. Текущая таблица
SMO class ID проверяется непосредственно против registrations:

```powershell
python -B research\inspect_executable_architecture.py `
  local-data\pc-pristine\WinxClub.exe `
  --class-id-table docs\reference\class-ids.md
```

Проверка уже обнаружила и исправила три старых удобных, но неоригинальных имени:
`0x63FEA321` — `spShadowVolumeManager`, `0x04680BC1` —
`spDXShadowVolumeManager`, `0x774E52E3` — `spDXShadowMeshSerializer`.

## Следующий доказательный шаг

Ограниченный пилот уже прошёл три уровня:
[`spBaseObject`](../research/native-class-sp-base-object.md),
[`spCrossPlatform`](../research/native-class-sp-cross-platform.md) и
[`spStream`](../research/native-class-sp-stream.md). Для stream-границы уже
зафиксированы обе registration/vtable, общий layout `0x1C`, девять pure-virtual
операций и основные невиртуальные wrappers; недоказанные имена по-прежнему не
назначаются как original API.

Подробный порядок от этого пилота до `SparkBase`, bootstrap и resource pipeline
зафиксирован в [плане реконструкции классов](../research/native-reconstruction-plan.md).

1. Углублять минимальную связку `spBaseObject -> registration/spRTTIManager ->
   spNamedObject -> spCrossPlatform -> spStream`, проверяя новые находки против
   канонического списка неизвестного.
2. Найти bootstrap `spApp/spEngineCore -> wx...`, порядок создания managers и
   virtual callbacks между движком и игрой.
3. Привязать функции loader/serializer/render pipeline к их registration owners,
   не переименовывая неизвестные `sub_<VA>`.
4. Сопоставить все интересующие SMO class ID с полным native type catalog и
   отметить application extension points.
5. После этого создать каркас реконструкции только из доказанных модулей и
   exact class names; неподтверждённые методы оставить адресными заглушками.
