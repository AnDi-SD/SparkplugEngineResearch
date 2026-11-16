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
`WxButton`, `WxEntity`, `CollectibleGem`, `HUDPart`, `GameTimer`, `LogoAnimGameFlowState`,
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

Suite `ContactRawDotRows` проверяет [одну строку и raw dot-ветвь](../../docs/engine/physics/contact-raw-dot-row.md): staged raw384B и отдельный diagnostic выбора записи до F32, ordered nearest64 и прежние kernels. Прежние physics/AI consumers проверяются отдельно; native graph, ABI, lifetime и общий solver остаются открытыми.

Suite `ContactTwoBodyRawDotRows` проверяет [одну строку и две raw dot-суммы](../../docs/engine/physics/contact-two-body-raw-dot-row.md). Операция выдаёт staged raw480B и отдельный diagnostic выбора записи до F32, сохраняет порядок двух nearest64-сумм и первичные x87 spill. Границы native graph, ABI, lifetime и общего solver остаются открытыми.

Suite `PlayerTraitsReset` проверяет [сброс captured prefix PlayerTraits](../../docs/game/characters/player-traits-reset.md), полную sparse value-проекцию 589 байт, default refusal, alias и идемпотентность. Полный native объект, lifecycle и gameplay остаются открытыми.

Suite `CharacterTraitsProperty` проверяет [raw-свойство captured prefix CharacterTraits](../../docs/game/characters/character-traits-property.md), raw read/write/readback-проекцию 424 байт и двух DWORD, default refusal, alias и повторную запись. Полный native объект, lifecycle и gameplay остаются открытыми.

Suite `MovingPlatformNear` проверяет [global0 near-state captured MovingPlatform](../../docs/game/entities/moving-platform-near-state.md), raw-проекцию 361 байт receiver, 12 байт cached Node position и 36 байт point, AL1, finite/global0 отказ и alias. Полный native объект, lifecycle и gameplay остаются открытыми.

Suite `CylindricalDoorProperty` проверяет [два именованных byte-свойства CylindricalDoor](../../docs/game/entities/cylindrical-door-properties.md), raw DWORD-to-byte truncation, getter readback, соседнее поле и полный375-байтный captured prefix, alias и явный отказ. Native bool/signature, sizeof, lifecycle и gameplay остаются открытыми.

Suite `SwampPlatformRingProperty` проверяет [четыре scalar-свойства SwampPlatformRing](../../docs/game/entities/swamp-platform-ring-properties.md), raw DWORD stores, фактические10-байтные FLD returns,308-байтные prefixes, cross-field preservation и явный finite-profile отказ. Native return signature, sizeof, lifecycle и gameplay остаются открытыми.

Suite `ChallengeParamsDefaults` проверяет [captured interior parameter defaults](../../docs/game/entities/challenge-params-defaults.md), 19 raw DWORD записей внутри префикса 368 байт, сохранение 292 opaque байт, alias, повторный вызов и отказ профиля. Native sizeof, конструктор, lifecycle и gameplay остаются открытыми.

Suite `DamageProxyProperty` проверяет [два raw DWORD-свойства DamageProxy](../../docs/game/entities/damage-proxy-properties.md), полные32-битные getter results и два304-байтных after-prefix, cross-field/opaque preservation, alias и явный профильный отказ. Current ctor защищён; native sizeof/типы, lifecycle и gameplay остаются открытыми.

Suite `RacingCheckpointProperty` проверяет [два raw DWORD-свойства RacingCheckpoint](../../docs/game/entities/racing-checkpoint-properties.md), полные32-битные getter results и два316-байтных after-prefix, cross-field/opaque preservation, alias и явный профильный отказ. Current ctor защищён; native sizeof/типы, lifecycle и gameplay остаются открытыми.

Suite `PendulumProperty` проверяет [два raw DWORD-свойства Pendulum](../../docs/game/entities/pendulum-raw-properties.md), полные32-битные getter results и два300-байтных after-prefix, cross-field/opaque preservation, alias и явный профильный отказ. Current ctor защищён; native sizeof/типы, lifecycle и gameplay остаются открытыми.

Suite `FallingRockProperty` проверяет [четыре raw DWORD-свойства FallingRock](../../docs/game/entities/falling-rock-properties.md), восемь полных32-битных getter results и четыре396-байтных after-prefix в порядке GET/SET/GET каждого свойства. Сохраняются opaque/cross-field bytes, alias и явный профильный отказ; current ctor, native sizeof/типы, lifecycle и gameplay остаются открытыми.

Suite `SwampGazBubbleDamage` проверяет [одно raw DWORD-свойство SwampGazBubble](../../docs/game/entities/swamp-gaz-bubble-damage.md), два полных32-битных getter results и один336-байтный after-prefix в порядке GET/SET/GET. Сохраняются opaque/cross-field bytes, alias и явный профильный отказ; current ctor, native sizeof/типы, lifecycle и gameplay остаются открытыми.

Suite `GlyphFacing` проверяет [Facing callback Glyph с no-op setter и FLD getter](../../docs/game/entities/glyph-facing-no-op.md), два10-байтовых x87 getter payload и один неизменённый352-байтный prefix в порядке GET/no-op/GET. Сохраняются opaque/cross-field bytes, alias и явный профильный отказ; current ctor, native sizeof/типы, lifecycle и gameplay остаются открытыми.

Suite `SideQuestGivenObjectScale` проверяет [Scale callback SideQuestGivenObject с записью F32 и FLD getter](../../docs/game/entities/side-quest-given-object.md), два10-байтовых x87 getter payload и один308-байтный after-prefix в порядке GET/SET/GET. Сохраняются opaque bytes, alias и явный finite-normal/signedzero профиль; current ctor, native sizeof/типы, lifecycle и gameplay остаются открытыми.

Suite `BreakableVaseValue` проверяет [raw Value of Fairy Stars у BreakableVase](../../docs/game/entities/breakable-vase-value.md), два полных32-битных getter results и один384-байтный after-prefix в порядке GET/SET/GET. Сохраняются opaque bytes, alias и профильный отказ; current constructor, sizeof/native types, применение значения и gameplay остаются открытыми.

Suite `ThunderMakerProperty` проверяет [две rawDWORD пары Delay у ThunderMaker](../../docs/game/entities/thunder-maker.md), шесть полных32-битных getter values и два304-байтных after-prefix в восьми последовательных callback на одном предоставленном состоянии. Numeric8hex слова, opaque bytes, cross-property сохранение и alias остаются явными; current constructor, native types/sizeof, порядок min/max, единицы, производство погоды и gameplay неизвестны.

Suite `EyeBallSentryDamage` проверяет [одну rawDWORD пару Damage у EyeBallSentry](../../docs/game/entities/eye-ball-sentry-damage.md), два полных32-битных getter values и328-байтный after-prefix в GET/SET/GET на одном предоставленном состоянии. Слова вводятся и выводятся как четыре little-endian байта памяти; opaque bytes и alias сохраняются. Historical416B allocation не доказывает sizeof; current constructor, native types/signedness/units, применение Damage, другие FLD/byte свойства и gameplay неизвестны.

Suite `KikoHoleRadius` проверяет [одну Radius F32-store/FLD пару KikoHole](../../docs/game/entities/kiko-hole-radius.md), два фактических10-байтных extended getter payload и полный324-байтный after-prefix в GET/SET/GET. Setter сохраняет все32 raw бита аргумента; профиль допускает finite normals или signed zero. Сохраняются opaque bytes и alias; historical332B allocation, неизвестный current constructor, sizeof/native ABI/единицы/lifecycle/production callers/gameplay остаются отдельными границами.

Suite `CabinetPuzzleProperty` проверяет [четыре rawDWORD пары цифр CabinetPuzzle](../../docs/game/entities/cabinet-puzzle-digits.md), восемь полных EAX getter values и четыре372-байтных after-prefix в12 callback на одном предоставленном состоянии. Все32 бита сохраняются; слова ввода/вывода имеют порядок четырёх little-endian байтов памяти. Названия Digit не задают диапазон0..9, signedness или механику пазла; historical468B allocation отдельно от minimum372B, constructor/sizeof/physicalinheritance/nativeABI/lifecycle/productioncallers/gameplay неизвестны.

[Архитектура общего ядра](../shared-core.md) · [Правила разработки](../../CONTRIBUTING.md).
