# Native camera → Remix: контракт и отсутствующий транспорт bridge

12 сентября 2026. Отдельная read-only проверка этапа 2
[перехода к прямой сцене](winx-remix-direct-scene-plan-2026-09-12.md).
Игра не запускалась, DLL/конфигурации/общие исходники не изменялись.

**Матрицы камеры уже доступны до D3D без восстановления по draw. Но установленный
x86 client не предоставляет `SetupCamera`: указатель равен нулю.**
64-битный renderer этот API предоставляет. Текущий D3D `SetTransform` остаётся
рабочим переходным транспортом; наличие native матриц само по себе не означает
готовой прямой передачи камеры.

## Подтверждённый источник и момент чтения

| Данные / операция | PC адрес или поле | Основание |
|---|---|---|
| Engine singleton | `[0x755274]` | существующая live диагностика |
| Current scene / main camera | `engine+0x18 / +0x1C` | scene observer и live совпадение |
| Полный camera layout | `spCameraObservedLayout`, размер `0x238` | exact native allocations и shared ABI |
| World→view | `camera+0xCC`, 16 float32 | actual аргумент native renderer |
| View→projection | `camera+0x10C`, 16 float32 | actual аргумент native renderer |
| Dirty flags | `camera+0x224`, view bit1 / projection bit2 | original apply |
| View / projection cache update | `0x427000 / 0x426F10` | прямые вызовы original apply |
| Apply camera | `0x427D40`, thiscall без аргументов, результат AL | body `0x5A` bytes и прежний CP6 |
| Renderer secondary interface | `renderer+0x18`, vtable `0x6F28A0` | CP47 |
| View setter | slot13 `0x4BBB20` → complete renderer `+0xCA80` → D3D transform2 | CP47, свежая проверка bytes |
| Projection setter | slot12 `0x4BBAE0` → `+0xCAC0` → D3D transform3 | CP47, свежая проверка bytes |

Общая структура находится в `Sparkplug/Analysis/PC/SparkplugAbi.h:1164`;
offset assertions — строки 2138–2150. Live объект нельзя приводить к
portable C++ классу: копировать подтверждённый ABI carrier на том же потоке.

`427D40` сначала обновляет dirty view/projection, затем передаёт view,
при успешном AL — projection; возвращает нормализованный результат projection.
Для наблюдения готовых матриц пригоден успешный возврат этого apply.
Чтение до вызова может захватить старые caches. Копирование из renderer
после последующего UI/shadow apply уже потеряет identity основной камеры.

Проверены равные bytes pristine EXE `3F022480…` и установленного debug EXE
`C27EA9DB…` для apply и обоих setters. Это переиспользует behavioural CP6/47;
не повторяет эмуляцию и не выдаёт static сравнение за новую live проверку.
Подробности: [камера](native-class-sp-camera.md),
[matrix inputs CP47](native-pc-renderer-matrix-inputs.md),
[предыдущий live camera contract](winx-remix-camera-contract-2026-09-12.md).

`viewAngle` игры горизонтальный; не надо переводить его в SDK vertical FOV,
если передаём готовую матрицу. Не использовать fresh projection utility
`spCamera::BuildProjectionMatrixForAnalysis` для подмены native cache:
у original orthographic branch есть подтверждённое сохранение остальных cells.
`twoDimensional+0xC8` и `projectionBranch+0x231` — разные поля.

## SDK и фактические установленные binaries

`public/include/remix/remix_c.h:386` задаёт `remixapi_CameraInfo`:
`sType`, `pNext`, `type`, `view[4][4]`, `projection[4][4]`.
`PFN_remixapi_SetupCamera` — `__stdcall`, аргумент `const CameraInfo*`,
результат `remixapi_ErrorCode`. `WORLD` соответствует `CameraType::Main`,
остальные поддержанные типы — SKY и VIEW_MODEL.

Для первого пути достаточно `pNext=nullptr` и 128 исходных bytes матриц.
`convert::toRtCamera` (`src/dxvk/rtx_render/rtx_remix_api.cpp:498–529`)
строит `Matrix4{info.view}` / `Matrix4{info.projection}`; соответствующий
constructor (`src/util/util_matrix.h:62`) копирует четыре последовательных
вектора. **Дополнительное транспонирование, инверсия или смена handedness
на входе не нужны.** Это согласуется с уже проверенным D3D→Remix совпадением.

| Установленный компонент | Static результат |
|---|---|
| x86 `d3d9.remix-original.dll`, SHA256 `A9D0846720E90D36D19AFB67E76A4D894EB349ECF13B847DE0CEDA4861669965` | initializer RVA `0x599F0`; interface size `0x58`; SetupCamera at offset `0x14` остаётся NULL |
| x64 `.trex/d3d9.dll`, SHA256 `F7C310821AA98BCDFDEC120330B0A89457B7C5EBA58D21464AF32639611C809F` | initializer RVA `0x1EDB70`; SetupCamera at offset `0x28` назначается RVA `0x1E7890` |

x86 success path RVA `59AC1..59BAD` сначала обнуляет interface, записывает
поддерживаемые pointers и копирует весь объект в output. Записи в поле
SetupCamera нет. Из Remix API функций DLL экспортирует только
`remixapi_InitializeLibrary` и `remixapi_RegisterCallbacks`; отдельного
camera export для обхода NULL нет. Это вывод из установленного бинарника,
а не только из SDK или комментария.

В reference `bridge/src/client/remix_api.cpp:445` назначение SetupCamera
закомментировано. В `bridge/src/util/util_commands.h:41–51` и
`bridge/src/server/main.cpp:2802–3163` нет соответствующей transport command.
В `util_remixapi.h:130–131` присутствуют только mappings CameraInfo→sType;
это **не сериализатор и не рабочий транспорт**.
Отсутствие hidden команды во всём установленном server binary отдельно
не доказывалось; публичного client entrypoint в любом случае нет.

Серверный `remixapi_SetupCamera` (`rtx_remix_api.cpp:1062`) проверяет
registered device и sType, отключает near-plane override и ставит
`processExternalCamera` в ту же command stream. Сам вызов не делает Present.
Наличие установленного function pointer ещё не проверяет live принятие
конкретных Winx matrices; такой вызов в этой работе не выполнялся.

## Порядок кадров и камеры других проходов

`RtCamera::update` (`rtx_camera.cpp:620`) отвергает второе обновление той же
камеры в одном frame. Поэтому explicit WORLD должен попасть **до первого
captured main draw**, иначе SUCCESS enqueue не доказывает применение.
Хорошая точка подключения — после успешного native `427D40`, пока original
ещё не начал draws. Проверять camera identity по engine/main и принадлежность
сцены/target; сам по себе перспективный вид не доказывает main camera.
Порядок target binding в этой точке во всех игровых путях ещё не измерен.

Visibility manager может иметь override camera `+0x38`. Уже наблюдались
несколько selection calls за кадр, в том числе отдельная ортографическая
камера после основной. Не брать «последнюю камеру», последний vector или
viewport dimensions как универсальный идентификатор WORLD. Sky/main/UI,
shadow и render-to-texture не должны перезаписывать явную основную камеру.
На первый этап допускается только подтверждённая основная мировая camera;
другие роли остаются отдельной частью плана.

Сохраняются original D3D camera setters для UI/fallback и ровно тот же
Present lifecycle. `winx_d3d9_probe.cpp:697–722` вызывает original device
slot17 один раз, `:780–790` — swapchain slot3. Не добавлять SDK Startup,
второй device или SDK Present. В установленном x86 interface Present также NULL.

Есть тонкость проверки: `processExternalCamera` (`rtx_camera_manager.cpp:177`)
вызывает только update. Запись Camera Sequence и обновление
`m_lastCameraCutFrameId` находятся в обычном `processCamera`, а не здесь.
Если external update первым занял frame, прежний Camera Sequence recorder
может не записать его через ordinary path. Поэтому прежний инструмент
не следует считать единственным oracle для прямой камеры. Переходы/смена
camera и сброс temporal history требуют отдельной live проверки.

## Минимальное общее расширение bridge

Рекомендуется изменить собственную отдельную копию **client+server bridge**,
сохраняя stock x64 DXVK renderer. Не патчить отсутствующий pointer на
адрес из другого процесса. Разные разрядности и процессы не позволяют
прямо вызвать серверный RVA из x86 adapter.

1. `bridge/src/util/util_commands.h`: добавить `RemixApi_SetupCamera` в конец
   enum, сохранив числовые IDs существующих D3D/API commands; добавить toString.
2. `bridge/src/util/util_remixapi.h/.cpp`: добавить shared serializer
   CameraInfo с явными sType/type/32 float32, без process-local pointers и
   нативного padding. Сначала поддержать null pNext; неподдержанную extension
   отклонять до отправки. Простое добавление alias `Serializable<T,true>`
   недостаточно: `_calcSize/_serialize/_deserialize` требуют специализаций.
3. `bridge/src/client/remix_api.cpp`: проверить info/sType/type/pNext, отправить
   одну command со snapshot данных через существующий `ClientMessage`,
   назначить interface.SetupCamera. Snapshot должен жить независимо от stack
   после возврата вызова.
4. `bridge/src/server/main.cpp`: принять и проверить payload size, собрать
   локальный x64 CameraInfo с null pNext, вызвать `g_remix.SetupCamera`,
   записать реальный server result при диагностике. Client SUCCESS обычно
   означает enqueue; не выдавать его за server/GPU success.
5. Дополнить существующие `bridge/test/rtx/unit/test_remix_api_write.cpp`,
   `test_remix_api_read.cpp` и common cases: x86→x64 и обратный round trip,
   разные float bit patterns, отсутствие передаваемых pointers, malformed
   размер/type/extension без рассинхронизации последующей команды.

Для минимальной camera версии не нужны skinning, параметрический FOV API,
изменение renderer или новая IPC queue. Подключать согласованную пару
client/server; совпадение старых command IDs не заменяет проверку общей
совместимости обновлённого bridge. Предусмотреть проверку capability/версии
до первого camera packet, чтобы несовместимая пара не расходилась по очереди.

## Наличие исходников и условия сборки

Read-only reference HEAD `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`
от 4 июня 2026; shallow repository. Ещё доступен `origin/main`
`39eba5c245cbfc84c3d5cd3e56ead56fd714c115` от 10 сентября.
В обоих snapshots bridge SetupCamera отключён. Локальная история содержит
только эти shallow tips; утверждать отсутствие исправления во всей публичной
истории нельзя. `68edea01` локально отсутствует.

Установленные client и NvRemixBridge имеют строку `remix-main+68edea01`,
renderer — `remix-1.5.2+68edea01`. Reference **не точный исходник installed
binaries**. До пересборки предпочтительно получить соответствующий source
commit или явно квалифицировать замену пары bridge по reference. Fetch,
checkout, clone и сборка в этой проверке не выполнялись.

`bridge/README.md` требует Windows, Python, Meson и MSVC x86/x64; документ
проверял VS2019/v142. `meson.build` требует Meson>=0.58, использует C++17,
Windows libraries Version/Comctl32 и штатные SDK headers; renderer submodules
RTXDI/NRC/USD для отдельного bridge по его build graph не требуются.
`enable_tracy=false` по умолчанию; bundled Tracy всё равно входит в проект.

На машине найдены VS `18/Community` и MSVC `14.51.36231`, `14.52.36725`.
Штатный `bridge/build_common.ps1:44` ищет версии `[16.0,18.0)` и поэтому
эту установку не выберет. Собственный build wrapper может использовать
уже работающий в проекте unrestricted vswhere + vcvarsall; совместимость
bridge с новыми MSVC пока не проверена. Meson/Ninja не найдены в текущем
PATH и Python packages; это не проверка всех возможных установок на диске.
Базовый план сборки: только release x86 client и x64 server с ограниченным
числом workers, затем cross-architecture serializer tests. Не запускать
`build_bridge_all.bat`, который собирает шесть сочетаний без нужды.

## Проверка перехода и сохранённое evidence

Сначала логировать capabilities установленного API (включая NULL camera),
собирать native camera packet и сверять его bitwise с D3D matrix values
на конкретном main draw, сохраняя game frame/camera/scene identity.
Это полезный переходный результат без ложного заявления о прямом SetupCamera.
После готовности bridge: server receive/result log с hash128 input bytes,
одна WORLD camera за frame до первого world submission; затем Alfea27 и
Domino4, движение/поворот, UI/загрузка/смена сцены, screenshot и проверка
actual accepted camera. Gardenia2 не запускать. A/B отключения camera API
сохраняет original D3D path и помогает отделить матрицы от temporal lighting.

Локальные артефакты не входят в обычный clone:
`local-data/rtx-remix/direct-scene-camera-20260912/inspect_contract.py` и
`static-contract.json`. Последний SHA256:
`3837288FCC982EE5F4EDEFD0A781444FBF62DB3E0B281A6D0AB49481ABF9B455`.
Он содержит hashes трёх установленных компонентов, exports, ограниченный
disassembly initializers, одинаковые native regions, source hashes и git facts.
Это static evidence; новый GPU/camera runtime test здесь не заявляется.
