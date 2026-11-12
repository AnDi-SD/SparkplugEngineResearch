# SparkplugViewer.Native

Наш C ABI поверх общих восстановленных C++-классов `Sparkplug/` и `Winx/`. Мост предоставляет handles, проверку входных размеров и управление временем жизни. Он не загружает игровой EXE.

## Интерфейс

[ViewerBridge.h](ViewerBridge.h) объявляет операции контейнера, объектного графа, mesh/texture inspection, анимации, узлов, skin и подготовки сцены. Дополнительные заголовки выделяют группы ABI. Managed-обвязка находится в `tools/SmoViewer/SmoViewer.Sparkplug`, Python-обвязка — в `tools/SanToVmd/sparkplug_native.py`.

Индексы ABI — host-индексы, а не оригинальные указатели. Входные массивы заимствуются на время вызова; runtime/clip handles владеют соответствующим состоянием. Исключения C++ перехватываются на границе ABI. SafeHandle и сериализованный native gate находятся в managed-слое.

Общие C++-классы читают и исполняют поддерживаемую игровую логику. Envelope encoding, транспорт данных и другие собственные host-операции маркируются отдельно. Полный перечень границ конкретного вызова определяется его объявлением и реализацией; наличие моста не означает поддержки всех классов и всех ветвей.

## Сборка

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/SparkplugViewer.Native/Build-Native.ps1 -Configuration Release -BuildWorkers 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/SparkplugViewer.Native/Build-Native.ps1 -Configuration Release -BuildWorkers 1 -RunChecks -CheckSuites FullLoader
```

Команды выполняются из корня репозитория. Нужны Visual Studio C++, Windows SDK и CMake. DLL x64 со статическим CRT создаётся в `artifacts/native/viewer/Release/`. `-Fresh` обновляет CMake-конфигурацию; `-BuildWorkers` принимает 1…4. `-CheckSuites` выбирает затронутые suites из списка в скрипте.

Если автоматический поиск Visual Studio недоступен, `-VisualStudioPath` задаёт существующую установку явно. Приложение должно использовать DLL, совместимую с текущей обвязкой.

Для восстановленных компонентов доступны также `GUIObject`, `Entity`,
`VectorWrapper`, `Perception`, `GameFlowState`, `GameFlowNavigation`,
`LoadSaveGameFlowState`, `DebugDialogGameFlowState`, `CreditsMenuGameFlowState`,
`IdleAIAction`, `MinotaurDefendAIAction`, `CentreScreenGameFlowState`,
`WxButton`, `WxEntity`, `HUDPart`, `GameTimer`, `LogoAnimGameFlowState`,
`MessageQueue`, `Notification`, `SleepingAIAction`, `TrollBeforeFightAIAction`,
`TargetManager`, `WxButtonNotify`, `MenuGameFlowState`
и существующий общий `SparkBase`.
Для отдельных ограниченных PC-контрактов доступны `ProjectionIndex`,
`KikoHoleAIAction`, `DroidWanderAIAction`, `GhoulScriptAIAction`,
`HelpAIAction`, `WanderAIAction`, `CharacterRegistry`, `WinxFlyAIAction`,
`DarcyAttackAIAction`, `StormyAttackAIAction`, `IcyAttackAIAction`, `MosquitoAttackAIAction`, `CommandExitAIAction`, `FrogAttackAIAction`, `IceWormAttackAIAction`, `TrollKnutAttackAIAction`, `SpiderShadowAttackAIAction`, `KikoMovingAIAction`, `IceWormHolesAIAction`, `MikaelWandringAIAction`, `PixelShader`, `TextureManager`, `GameOverGameFlowState`, `CinematicGameFlowState`, `TextController`, `AudioBank`,
`AudioBankEntry`, `AudioBankOwnedEntries`, `AudioManagerBank`,
`AudioManagerOwnedEntries`,
`CameraManager`, `CinematicManager`, `BaseReferrers`, `PressStartGameFlowState` и `Timer`.
Эти suites собирают общие
исходники и проверяют их portable контракты; они не запускают оригинальную
игру и не подтверждают поддержку новых компонентов в публичном C ABI.

Отдельный `SphereContact` проверяет общий [PC value helper](../../docs/engine/physics/sphere-contact.md): строгое касание, точные выходные слова, сохранение результата при отсутствии контакта и явный конечный профиль арифметики. Это самостоятельная аналитическая функция с неизвестным исходным владельцем; suite не подтверждает wrapper, dispatcher или целый класс.

Отдельный `ContactConstraint` проверяет [captured PC-операцию `1C`](../../docs/reference/classes/sp-contact-constraint.md): полное raw-состояние, конечные коэффициенты, знаковые нули, subnormal, sentinel `FLT_MAX` и явный отказ вне аналитического профиля. Исходная сигнатура возврата, базовый layout, solver, жизненный цикл и целый класс остаются открытыми.

Suite `ConstraintValueQueries` проверяет общий [PC tangent value helper](../../docs/engine/physics/contact-tangents.md) с явным конечным профилем и отдельным отказом по умолчанию. После изменения общего Number-root также требуется `SphereContact`; имя native C++ владельца и интеграция solver остаются открытыми.

Suite `ContactRows` проверяет [ограниченную однострочную PC-операцию](../../docs/engine/physics/contact-single-body-row.md): staged raw384B, точные Manager7C/80 и явный конечный профиль. Общая арифметика требует также `SphereContact` и `ConstraintValueQueries`; `ContactConstraint` сохраняет отдельные ingress/finite-update проверки. Это не квалификация native ABI, startup или общего solver.

Suite `ContactTwoBodyRows` проверяет [строку с двумя заданными body](../../docs/engine/physics/contact-two-body-row.md): staged raw480B, 14 finite inputs, точную асимметрию x87 и отказ по умолчанию. Прежние `ContactRows`, `ContactConstraint`, `SphereContact` и `ConstraintValueQueries` проверяются отдельно. Native graph, ABI, constructor и общий solver не квалифицированы.

Suite `ContactMultirows` проверяет [две и три строки одного supplied body](../../docs/engine/physics/contact-single-body-multirow.md): staged raw384B, полный count2/3, сохранение padding/RHS и F32 tangent rereads на прежних nearest64 kernels. Прежние physics и AI consumers проверяются отдельно; native graph, ABI, constructor и общий solver остаются открытыми.

Suite `ContactProvidedTangents` проверяет [строки с заданным tangent](../../docs/engine/physics/contact-provided-tangent-rows.md): отдельный staged raw384B, полный07E5-профиль, normal FCHS/F32, supplied tangent и F32 cross rereads, opaque DWORD overrides и отказ по умолчанию. Прежние Multirow/physics/AI consumers проверяются отдельно; native graph, ABI, lifetime и общий solver остаются открытыми.

Suite `ContactTwoBodyMultirows` проверяет [две и три строки с двумя заданными body](../../docs/engine/physics/contact-two-body-multirow.md): staged raw480B, конечный профиль, различие primary/additional J2, positive T2 и точные F32 spill/reload на прежних kernels. Прежние ProvidedTangent/physics/AI consumers проверяются отдельно; native graph, ABI, lifetime и общий solver остаются открытыми.

[Архитектура общего ядра](../shared-core.md) · [Правила разработки](../../CONTRIBUTING.md).
