# Полный каталог базы знаний

Все публичные статьи сгруппированы по темам. [Начальная страница](README.md) предлагает маршрут чтения, [каталог классов](reference/classes.md) — поиск типа или ID.

## Начало

- [База знаний Sparkplug и Winx Club](README.md).

## Движок

- [Движок Sparkplug](engine/README.md).

## Движок / Анимация и скелеты

- [Decoded light first-generation bounded candidate](engine/animation/skin-decoded-light-generated-boundary.md).
- [GUIObject: поиск GUICollision и история состояния контроллеров](engine/animation/gui-controller-binding.md).
- [MovieTextureController и конкретные VideoStream](engine/animation/movie-controller-runtime.md).
- [PC `spActor`: playback, события и граница менеджера](engine/animation/actor-playback.md).
- [PC `spAnimation`, `spTrack`, `spAnimTrack`: object lifetime](engine/animation/animation-lifecycle.md).
- [PC actor: capacity, queries и fade-stop](engine/animation/actor-controls.md).
- [PC actor: discovery, input insertion и запуск](engine/animation/actor-binding.md).
- [PC actor: full-capacity Start и fade-stop до реального Tick](engine/animation/actor-control-pipeline.md).
- [PC actor: перенос owned runtime и сквозная проверка](engine/animation/actor-owned-runtime.md).
- [PC actual selected light cache through complete Skin draw](engine/animation/skin-selected-light.md).
- [PC alpha flush → actual RenderNode → owning decoded Skin refusal](engine/animation/skin-alpha-flush.md).
- [PC animation runtime: SAN → node → skin palette](engine/animation/animation-runtime.md).
- [PC FunctionEval: scalar runtime and shared codec (checkpoint21)](engine/animation/function-eval.md).
- [PC queued Skin first-generation: bounded failure record](engine/animation/skin-queued-generated-boundary.md).
- [PC real SAN → retained Skin → first shader generation](engine/animation/skin-san-generated-render.md).
- [PC real SAN/scene world + decoded mesh → Skin draw](engine/animation/skin-scene-mesh-render.md).
- [PC SAN actor → прочитанная кость → Skin palette/draw](engine/animation/skin-san-render.md).
- [PC SAN → scene world → Skin shader generation/draw](engine/animation/skin-scene-generated-render.md).
- [PC SAN: payload → descriptors → preparation → sampling](engine/animation/animation-keys.md).
- [PC Skin-owned Fog/material/bone → SAN/scene/mesh draw](engine/animation/skin-fog-render.md).
- [PC Skin: actual light world to shader constants](engine/animation/skin-light-constants.md).
- [PC Skin: complete light submission in decoded draw](engine/animation/skin-lights-render.md).
- [PC Skin: palette -> mesh -> shader constants -> draw](engine/animation/skin-render.md).
- [PC Skin: сохранённый граф read → render](engine/animation/skin-loaded-render.md).
- [PC unchanged SMO light through complete lit Skin draw](engine/animation/skin-decoded-light.md).
- [PC прочитанный Skin → generating shader miss → draw](engine/animation/skin-generated-render.md).
- [PC: queued Skin draw and mesh bounds](engine/animation/skin-queued-mesh-render.md).
- [Анимация и скелеты](engine/animation/README.md).

## Движок / Архитектура и жизненный цикл

- [Keyboard/Mouse: исходные состояния и различия PC/PS2](engine/architecture/input-cached-state.md).
- [PC scene: SkyBox, Projection и LensFlare managers](engine/architecture/scene-special-managers.md).
- [PC spSceneManager: borrowed list и world caller](engine/architecture/scene-manager-world-source.md).
- [Архитектура Sparkplug](engine/architecture/overview.md).
- [Архитектура и жизненный цикл](engine/architecture/README.md).
- [Общие классы движка: дополнительный проход](engine/architecture/engine-core-remainder.md).
- [Платформенные классы движка: PC и PS2, 10 сентября 2026](engine/architecture/engine-platform-remainder.md).
- [Порядок обновления PC-кадра](engine/architecture/engine-frame.md).
- [Сеть PC: объекты и формат пакета](engine/architecture/network-family.md).
- [Таймеры PC и PS2: идентичность, состояния и границы часов](engine/architecture/timer-family.md).

## Движок / Свет, частицы и эффекты

- [LightController, ColorFuncEval и ссылки LightEntity](engine/effects/light-runtime.md).
- [PC decoded Fog → renderer identity/device cache](engine/effects/renderer-fog.md).
- [PC decoded light: complete no-scene virtual world callback](engine/effects/light-world.md).
- [PC decoded light: whole Scene attachment and world cache refresh](engine/effects/light-scene-world.md).
- [PC looping Particle Init: остановленная граница bg_particles](engine/effects/particle-loop-init-frontier.md).
- [PC particle looping Init: directed counts 127/128/129](engine/effects/particle-loop-init-counts.md).
- [PC ParticleSystem: генератор случайных чисел и области эмиссии](engine/effects/particle-sampling.md).
- [PC ParticleSystem: параметры, writer и первая целая сцена](engine/effects/particle-parameters.md).
- [PC real light graph: index, save references and fresh native load](engine/effects/light-graph-roundtrip.md).
- [PC renderer light submission](engine/effects/renderer-lights.md).
- [Свет, частицы и эффекты](engine/effects/README.md).
- [Точность преобразования ориентации света на PC](engine/effects/light-corpus.md).

## Движок / Геометрия и буферы

- [PC DXMeshData: полная запись CPU-геометрии](engine/geometry/mesh-writer.md).
- [PC MeshData: indexing и generic reference writer](engine/geometry/mesh-graph-writer.md).
- [PC общий MeshData writer](engine/geometry/mesh-base-writer.md).
- [PC: экспорт MeshData из исходников принимает original reader](engine/geometry/mesh-writer-roundtrip.md).
- [Геометрия и буферы](engine/geometry/README.md).
- [Цвет вершин до draw: PC/PS2 layout и PC materialization](engine/geometry/vertex-color-contract.md).

## Движок / Материалы и шейдеры

- [MaterialColorController: PS2 constructor и граница PC](engine/materials/material-color-ps2-constructor.md).
- [PC complete pass texture-state batch](engine/materials/material-pass-states.md).
- [PC decoded material + SAN/scene/mesh → Skin draw](engine/materials/skin-material-mesh-render.md).
- [PC full Skin material state protocol](engine/materials/skin-material-protocol.md).
- [PC material color,](engine/materials/material-color.md).
- [PC material controllers and texture tracks — checkpoint 20](engine/materials/material-controllers.md).
- [PC material installation and state batch](engine/materials/material-install.md).
- [PC material lighting and color sources](engine/materials/material-lighting.md).
- [PC material pass update and texture binding](engine/materials/material-pass-binding.md).
- [PC Material → ColorController → frame gate](engine/materials/material-color-graph.md).
- [PC material-state translation](engine/materials/material-state-map.md).
- [PC material: exact ABI, scalar codec and pass ownership](engine/materials/material-scalar.md).
- [PC RFX constants and pass persistence](engine/materials/rfx-constants.md).
- [PC RFX variable producer and ownership](engine/materials/rfx-variables.md).
- [PC RFX: файл, regex, ID, имя и создание template](engine/materials/rfx-file.md).
- [PC shader manager and renderer key](engine/materials/shader-manager-key.md).
- [PC shader parameter production boundary](engine/materials/shader-parameter-append.md).
- [PC shader resources: исходник, программа и численный результат](engine/materials/shader-contract.md).
- [PC shader template frontier and scope v2 correction](engine/materials/shader-template-frontier.md).
- [PC Skin-owned material + SAN/scene/mesh → draw](engine/materials/skin-owned-material-render.md).
- [PC spDXShaderLayer](engine/materials/shader-layer.md).
- [PC standard material graph and DX runtime identity](engine/materials/material-standard-graph.md).
- [PC template source and compiler-output consumer](engine/materials/shader-compile.md).
- [PC vertex shader and constant parameters/41](engine/materials/shader-constants.md).
- [PC: DX metadata, buffer materialization и declaration](engine/materials/dx-materialization.md).
- [RFX: от файла к shader](engine/materials/rfx-pipeline.md).
- [Whole PC shader-manager generating miss](engine/materials/shader-generation.md).
- [Материалы и шейдеры](engine/materials/README.md).

## Движок / Объекты, владение и уведомления

- [`spSubscriptionManager`: исправление по оригиналам PC и PS2](engine/objects/subscription-order-correction.md).
- [Audio: cached параметры менеджера и состояние голоса](engine/objects/audio-cached-parameters.md).
- [AudioBank: удаление, владение и восстановление default bank](engine/objects/audio-bank-ownership.md).
- [GamePad: состояния, диапазоны и нормализация PC/PS2](engine/objects/gamepad-cached-state.md).
- [Объекты, владение и уведомления](engine/objects/README.md).
- [Оригинальный helper ориентации и полная активация Kiko на PC](engine/objects/orientation-basis.md).

## Движок / Физика и коллизии

- [CollisionInfo и OBB: используемая инструментами часть PC](engine/physics/collision-tools-core.md).
- [MeshBV и данные граней для общих ядер tools](engine/physics/mesh-bv-tools-core.md).
- [Физика и коллизии](engine/physics/README.md).
- [Физика и коллизии Sparkplug](engine/physics/overview.md).
- [Физические объекты и BoundingVolume: PC и PS2](engine/physics/physics-family.md).

## Движок / Платформенные реализации

- [PC ColorFuncEval,](engine/platforms/color-functions.md).
- [PC material UV → TransFunctionEval → FunctionEval](engine/platforms/uv-functions.md).
- [PC SAN: запись анимации и вложенные блоки](engine/platforms/san-writer.md).
- [PC SAN: полный reader объекта `spAnimation`](engine/platforms/san-reader.md).
- [PC SAN: полный source-файл принимается оригинальным reader](engine/platforms/san-file-roundtrip.md).
- [PC shared DX payload: writer, cursor и failure boundary](engine/platforms/dx-shared-payload.md).
- [PC и PS2](engine/platforms/pc-and-ps2.md).
- [PS2 RNG: полная начальная подготовка и перестроение](engine/platforms/ps2-pc-rng-transactions.md).
- [Оставшиеся общие классы PS2 и контроллеры, 10 сентября 2026](engine/platforms/ps2-remainder.md).
- [Платформенные реализации](engine/platforms/README.md).

## Движок / Отрисовка

- [PC automatic draw: создание шейдера и повторное использование](engine/rendering/renderer-generated-draw.md).
- [PC cached automatic weighted draw](engine/rendering/renderer-cached-draw.md).
- [PC Clear / Present boundary](engine/rendering/renderer-present-clear.md).
- [PC complete cached weighted submission](engine/rendering/renderer-weighted-submit.md).
- [PC complete geometry submission](engine/rendering/renderer-submit.md).
- [PC material UV → renderer matrix submission](engine/rendering/uv-renderer.md).
- [PC preselected-shader draw and shader stacks](engine/rendering/renderer-draw.md).
- [PC Renderable callbacks и очереди renderer](engine/rendering/renderer-protocol.md).
- [PC renderer matrix cache](engine/rendering/renderer-matrices.md).
- [PC renderer matrix setters/raw getters](engine/rendering/renderer-matrix-inputs.md).
- [PC scene boundaries and real material payload](engine/rendering/renderer-scene.md).
- [PC: модель, render-узел и точные правила копирования](engine/rendering/model-render-world.md).
- [PC: целый SceneRender, Shadow manager и DX state cache](engine/rendering/scene-render-runtime.md).
- [Отрисовка](engine/rendering/README.md).
- [Проекции: геометрия, FX и разные интерфейсы PC/PS2](engine/rendering/projection-family.md).

## Движок / Ресурсы и сериализация

- [PC Fog codec, Model relationship and lifetime](engine/resources/fog-serialization.md).
- [PC Light and LightData complete scalar codecs](engine/resources/light-serialization.md).
- [PC Node serialization: fields → relationships → owned tree](engine/resources/node-serialization.md).
- [PC RenderNode → Renderable/Model serialization](engine/resources/scene-serialization.md).
- [PC save: индексация объектов и запись ссылок](engine/resources/save-reference.md).
- [PC Skin: фабрика и полный поток сериализатора](engine/resources/skin-serialization.md).
- [PC: whole loading.smo с текстурой и общей геометрией](engine/resources/whole-textured-scene.md).
- [PC: whole Skin scene и точность Node transform](engine/resources/whole-skin-scene.md).
- [PC: внешний materializer и переносимый full-file loader](engine/resources/full-loader.md).
- [PC: общий загрузчик SMO/SAN, FAT и фабрика классов](engine/resources/smo-san-loader.md).
- [PC: целый gem.smo, две текстуры и UV-контроллер](engine/resources/whole-uv-scene.md).
- [PC: целый общий SMO с runtime DXLight и DXMesh](engine/resources/whole-light-scene.md).
- [PC: чтение ссылок, inline materialization и кэш](engine/resources/read-reference.md).
- [Восстановление Clone восьми пространственных сериализаторов](engine/resources/spatial-serializer-clone-fix.md).
- [Дополнение независимого учёта сериализаторов PC/PS2](engine/resources/serializer-expansion.md).
- [Загрузка ресурсов и объектный граф](engine/resources/pipeline.md).
- [Ресурсы и сериализация](engine/resources/README.md).

## Движок / Сцена и преобразования

- [PC render-узел: сцена, world cache и граница отрисовки](engine/scene/render-node-runtime.md).
- [PC spNode: world-update и защищённые переходы](engine/scene/node-world.md).
- [PC сцена: регистрация, дерево узлов и кадр анимации](engine/scene/scene-world.md).
- [Сцена и преобразования](engine/scene/README.md).
- [Узлы и мировые преобразования](engine/scene/overview.md).

## Движок / Текстуры

- [compressed mip generation и exact block encoding](engine/textures/texture-compressed-mips.md).
- [PC decoded texture + whole first shader generation](engine/textures/skin-texture-generated-render.md).
- [PC decoded texture → material → whole SAN/scene/mesh/Fog/Skin draw](engine/textures/skin-texture-render.md).
- [PC DXT block decode и original missing-mip путь](engine/textures/texture-compressed-blocks.md).
- [PC material → texture: common reference graph (checkpoint 19)](engine/textures/material-texture-links.md).
- [PC native texture data: shared source wrapper and reconstructed reader](engine/textures/texture-native-source.md).
- [PC palette, texture ownership and runtime failure contracts](engine/textures/palette-lifetime.md).
- [PC texture/sampler state mapping](engine/textures/texture-state-map.md).
- [PC texture: runtime flat codec, native mip copy и настоящий registry key](engine/textures/texture-runtime-mips.md).
- [PC texture: source/local codec, DX header и границы upload](engine/textures/texture-codec-boundaries.md).
- [PC TextureData native writer and shared section core](engine/textures/texture-native-writer.md).
- [PC: недостающие mip-уровни raw TextureData](engine/textures/texture-missing-mips.md).
- [PC: общие raw pixels → DXTexture](engine/textures/texture-cross-upload.md).
- [внешний источник TextureData](engine/textures/texture-external-source.md).
- [Текстуры](engine/textures/README.md).

## Движок / Текст и интерфейс

- [GUI: сообщения Widget, Button, EditBox и wxButton](engine/ui/gui-message-routes.md).
- [GUIManager: порядок смены фокуса](engine/ui/gui-focus-handoff.md).
- [PC file -> owned text: общий helper RFX](engine/ui/parser-file-text.md).
- [Начальное состояние текста на PC](engine/ui/text-defaults.md).
- [Текст и интерфейс](engine/ui/README.md).
- [Уточнение Clone слоёв и контекста CloneManager, 10 сентября 2026](engine/ui/layer-clone-context.md).
- [Уточнение оставшегося объекта при проверке Projection](engine/ui/projection-context-followup.md).

## Движок / Видимость и пространственные структуры

- [PC `spBSPNode`: lifetime, queries и выбор Zone](engine/visibility/bsp-runtime.md).
- [PC `spOcclusionVolume`: геометрия и потребитель в Scene](engine/visibility/occlusion-runtime.md).
- [PC `spOctreeNode`: исходные запросы и частичный source](engine/visibility/octree-runtime.md).
- [PC `spZonePortal` / `spZonePortalNode`: геометрия и обход зон](engine/visibility/zone-portal-runtime.md).
- [PC BSP/Octree: static и occlusion consumers](engine/visibility/spatial-consumers.md).
- [PC polygon clipping: unnamed value helper и точные пограничные правила](engine/visibility/polygon-clipping.md).
- [PC: partition graph и статические render supports](engine/visibility/partition-runtime.md).
- [PC: SceneInit, partition transfer и spVisibilityManager](engine/visibility/visibility-runtime.md).
- [PC: массивы плоскостей видимости и общий блокер копирования](engine/visibility/visibility-plane-storage.md).
- [Видимость и пространственные структуры](engine/visibility/README.md).

## Форматы

- [SAN (`spAnimation` в FFPS)](formats/san.md).
- [SMO field mutation subsystem](formats/smo-field-mutation.md).
- [Игровые ресурсы помимо SMO](formats/game-resources.md).
- [Контейнер PCK](formats/pck.md).
- [Текстуры внутри SMO](formats/smo-textures.md).
- [Типы объектов SMO](formats/smo-objects.md).
- [Формат SMO / FFPS](formats/smo.md).
- [Формат STX](formats/stx.md).
- [Форматы ресурсов](formats/README.md).

## Игра

- [Игра Winx Club](game/README.md).

## Игра / Искусственный интеллект

- [`wxAIAction`: общий контракт и 35 производных действий](game/ai/ai-action-family.md).
- [AIAction: Copy изменённых полей](game/ai/ai-action-modified-copy.md).
- [AIAction: входы через реестр и сообщения владельцу](game/ai/ai-registry-entry.md).
- [AIAction: завершение команд, входные параметры и выбор состояния](game/ai/ai-action-command-protocol.md).
- [AIAction: настоящий virtual reset объекта команд](game/ai/ai-command-reset.md).
- [AIAction: начальные поля девяти атак](game/ai/ai-attack-entry.md).
- [AIAction: подтверждение команды состоянием персонажа](game/ai/ai-combat-handshakes.md).
- [AIAction: поиск цели и запрос другого действия](game/ai/ai-perception-control.md).
- [AIAction: поля входа и общая выбранная цель](game/ai/ai-action-entry-fields.md).
- [AIAction: случайные параметры Minotaur и Mosquito](game/ai/ai-random-entry.md).
- [Активация IceWormHoles и начальные связи Kiko](game/ai/ai-activation-fields.md).
- [Выход из двадцати AI-состояний](game/ai/ai-exit-cleanup.md).
- [Искусственный интеллект](game/ai/README.md).
- [Семейство wxBaseAIBehavior: конструкция и выбор действий](game/ai/ai-behavior-family.md).

## Игра / Игровые ресурсы

- [Игровые ресурсы](game/assets/README.md).
- [Неизвестные классы Алфеи и привязка анимаций](game/assets/alfea-unknown-resources.md).
- [Поиск игровых ресурсов на PC](game/assets/resource-loading.md).

## Игра / Персонажи и действия

- [CharacterState: 97 классов PC и PS2](game/characters/character-state-family.md).
- [CharacterState: разрешения и режимы переходов PC/PS2](game/characters/character-state-permissions.md).
- [CharacterState: управляющие записи и события анимации](game/characters/character-state-data-events.md).
- [CharacterState: формирование ключей и запросы анимации](game/characters/character-animation-requests.md).
- [CharacterStateMachine: создание и раскладка 36 классов](game/characters/character-machine-family.md).
- [wxAnimationController и spActor: команды и подготовка запуска](game/characters/character-animation-playback.md).
- [wxAnimationManager: поиск по packed key и индексированной таблице](game/characters/character-animation-selection.md).
- [Общий протокол CharacterStateMachine: переходы, стек, события](game/characters/character-machine-protocol.md).
- [Персонажи и действия](game/characters/README.md).
- [Система внешних волос Bloom](game/characters/bloom-hair-system.md).
- [Снаряды и менеджеры: два семейства PC/PS2](game/characters/projectile-families.md).
- [События анимации: завершение,текущее состояние и звук](game/characters/character-animation-events.md).

## Игра / Игровые объекты

- [CollectibleGem: flags и проверка близости](game/entities/collectible-proximity.md).
- [Entity: копирование изменённых полей 24 подклассов](game/entities/entity-modified-copy.md).
- [Entity: собственные predicates персонажа, анимации и расстояния](game/entities/entity-runtime-predicates.md).
- [Entity: ссылки, общие флаги и переход между менеджерами](game/entities/entity-reference-flags.md).
- [Entity: строки, векторы и блок параметров при Copy](game/entities/entity-aggregate-copy.md).
- [Knut: полный вход, поиск Node и cached эффекты](game/entities/knut-node-entry.md).
- [wxEntity и прямые связи регистрации: создание, копирование, раскладка](game/entities/entity-direct-family.md).
- [wxEntityManager: добавление и удаление указателей](game/entities/entity-manager-insertion-removal.md).
- [wxEntityManager: таблицы, размеры и выбор группы](game/entities/entity-manager-tables.md).
- [Игровые объекты](game/entities/README.md).
- [Оставшиеся игровые классы: независимый проход PC и PS2](game/entities/game-remainder.md).
- [Привязка Node: Barrel и назначение Copy у MoveCtrl](game/entities/entity-bound-node-copy.md).

## Игра / Состояния игры и меню

- [Debug menu и маршруты runtime-проверки Winx Club](game/flow/winx-debug-runtime-inventory.md).
- [DebugMenu, DialogWindow и LoadSave: короткие active hooks](game/flow/game-flow-small-hooks.md).
- [Game flow: навигация HUD и переназначение ресурсов PC/PS2](game/flow/game-flow-navigation-remap.md).
- [Разрешение экрана, камеры и GUI в Winx Club PC](game/flow/display-camera.md).
- [Семейство `wxGameFlowState`: PC lifecycle и PS2 construction](game/flow/game-flow-family-construction.md).
- [Семейство GUIObject: конструкция, состояния и сообщения](game/flow/gui-object-family.md).
- [Состояния игры и меню](game/flow/README.md).

## Игра / Триггеры и взаимодействия

- [DispelTrigger и ChestTrigger: условия действия](game/triggers/trigger-action-gates.md).
- [PivotingDoor, PushButton, SnowPile: выбор взаимодействия](game/triggers/trigger-interaction-gates.md).
- [Tick: пауза и переходы четырёх триггеров](game/triggers/trigger-tick-gates.md).
- [Yeti: полное начало атаки и рекурсивное отключение Node](game/triggers/yeti-node-enable.md).
- [Вход и выход триггеров: настоящие HUD-зависимости](game/triggers/trigger-entry-exit.md).
- [Триггеры и взаимодействия](game/triggers/README.md).
- [Триггеры: 43 класса и общая управляющая логика](game/triggers/generic-trigger-family.md).
- [Триггеры: реальные потребители геометрического helper](game/triggers/trigger-distance-predicates.md).

## Справочник

- [Каталог классов](reference/classes.md).
- [Классы движка](reference/engine-classes.md).
- [Классы игры](reference/game-classes.md).
- [Справочник](reference/README.md).

## Справочник / Карточки классов

- [spActor](reference/classes/sp-actor.md).
- [spAnimation / spAnimationSerializer](reference/classes/sp-animation.md).
- [spAnimationManager](reference/classes/sp-animation-manager.md).
- [spAnimTexController](reference/classes/sp-anim-tex-controller.md).
- [spAnimTexControllerSerializer](reference/classes/sp-anim-tex-controller-serializer.md).
- [spApp](reference/classes/sp-app.md).
- [spAsyncFileStreamManager](reference/classes/sp-async-file-stream-manager.md).
- [spBaseObject / spNamedObject](reference/classes/sp-base-object.md).
- [spBoxBV](reference/classes/sp-box-bv.md).
- [spBoxBVSerializer](reference/classes/sp-box-bv-serializer.md).
- [spBSPNode](reference/classes/sp-bsp-node.md).
- [spCamera](reference/classes/sp-camera.md).
- [spCameraDataSerializer](reference/classes/sp-camera-data-serializer.md).
- [spCameraSerializer](reference/classes/sp-camera-serializer.md).
- [spCollisionInfo](reference/classes/sp-collision-info.md).
- [spColorFuncEvalSerializer](reference/classes/sp-color-func-eval-serializer.md).
- [spController / spSubController](reference/classes/sp-controller.md).
- [spCrossPlatform](reference/classes/sp-cross-platform.md).
- [spDataBlockSerializer](reference/classes/sp-data-block-serializer.md).
- [spDebugManager](reference/classes/sp-debug-manager.md).
- [spDXCombinedVB](reference/classes/sp-dx-combined-vb.md).
- [spDXIndexBuffer / spDXVertexBuffer](reference/classes/sp-dx-buffers.md).
- [spDXInputManager](reference/classes/sp-dx-input-manager.md).
- [spDXLight](reference/classes/sp-dx-light.md).
- [spDXMaterialDataSerializer](reference/classes/sp-dx-material-data-serializer.md).
- [spDXMesh](reference/classes/sp-dx-mesh.md).
- [spDXMeshCombiner](reference/classes/sp-dx-mesh-combiner.md).
- [spDXMeshData](reference/classes/sp-dx-mesh-data.md).
- [spDXMeshDataSerializer](reference/classes/sp-dx-mesh-data-serializer.md).
- [spDXMeshSerializer](reference/classes/sp-dx-mesh-serializer.md).
- [spDXSharedMeshData](reference/classes/sp-dx-shared-mesh-data.md).
- [spDXTextureDataSerializer](reference/classes/sp-dx-texture-data-serializer.md).
- [spDXVertexDeclaration / spPCVertexDeclaration](reference/classes/sp-vertex-declaration.md).
- [spEngineCore](reference/classes/sp-engine-core.md).
- [spEngineCore / wxEngineCore](reference/classes/wx-engine-core.md).
- [spEntityManager](reference/classes/sp-entity-manager.md).
- [spError](reference/classes/sp-error.md).
- [spErrorManager](reference/classes/sp-error-manager.md).
- [spFileStream](reference/classes/sp-file-stream.md).
- [spFog](reference/classes/sp-fog.md).
- [spFogSerializer](reference/classes/sp-fog-serializer.md).
- [spFont](reference/classes/sp-font.md).
- [spFontManager](reference/classes/sp-font-manager.md).
- [spFunctionEvalSerializer](reference/classes/sp-function-eval-serializer.md).
- [spGameLevel](reference/classes/sp-game-level.md).
- [spGameLevelSerializer](reference/classes/sp-game-level-serializer.md).
- [spIndexBuffer](reference/classes/sp-index-buffer.md).
- [spInputManager](reference/classes/sp-input-manager.md).
- [spLensFlare](reference/classes/sp-lens-flare.md).
- [spLight](reference/classes/sp-light.md).
- [spLightControllerSerializer](reference/classes/sp-light-controller-serializer.md).
- [spLightData](reference/classes/sp-light-data.md).
- [spLightDataSerializer](reference/classes/sp-light-data-serializer.md).
- [spLightManager](reference/classes/sp-light-manager.md).
- [spLightSerializer](reference/classes/sp-light-serializer.md).
- [spMatColorControllerSerializer](reference/classes/sp-mat-color-controller-serializer.md).
- [spMaterial / spMaterialData](reference/classes/sp-material-runtime.md).
- [spMaterialColorController](reference/classes/sp-material-color-controller.md).
- [spMaterialData](reference/classes/sp-material-data.md).
- [spMaterialDataSerializer](reference/classes/sp-material-data-serializer.md).
- [spMaterialPassLayer / spMaterialTextureLayer / spStdLayer](reference/classes/sp-material-layers.md).
- [spMaterialSerializer](reference/classes/sp-material-serializer.md).
- [spMaterialTexture](reference/classes/sp-material-render-target-texture.md).
- [spMemoryStream](reference/classes/sp-memory-stream.md).
- [spMesh](reference/classes/sp-mesh.md).
- [spMeshBV](reference/classes/sp-mesh-bv.md).
- [spMeshData](reference/classes/sp-mesh-data.md).
- [spMeshDataSerializer](reference/classes/sp-mesh-data-serializer.md).
- [spMeshNavigationSet](reference/classes/sp-mesh-navigation-set.md).
- [spModel](reference/classes/sp-model.md).
- [spModelSerializer](reference/classes/sp-model-serializer.md).
- [spNavigationGraph](reference/classes/sp-navigation-graph.md).
- [spNavigationPortal](reference/classes/sp-navigation-portal.md).
- [spNode](reference/classes/sp-node.md).
- [spNodeController](reference/classes/sp-node-controller.md).
- [spNodeSerializer](reference/classes/sp-node-serializer.md).
- [spOBBBV](reference/classes/sp-obbbv.md).
- [spOBBBVSerializer](reference/classes/sp-obb-bv-serializer.md).
- [spOcclusionVolume](reference/classes/sp-occlusion-volume.md).
- [spOctreeNode](reference/classes/sp-octree-node.md).
- [spParser](reference/classes/sp-parser.md).
- [spParticleSystem](reference/classes/sp-particle-system.md).
- [spPartitionNode](reference/classes/sp-partition-node.md).
- [spPartitionRenderable](reference/classes/sp-partition-renderable.md).
- [spPartitionSystem](reference/classes/sp-partition-system.md).
- [spPCApp](reference/classes/sp-pc-app.md).
- [spPCAsyncFileStreamManager](reference/classes/sp-pc-async-file-stream-manager.md).
- [spPCEffectTemplate / spPCRFXFileLoader](reference/classes/sp-pc-effect-template.md).
- [spPCErrorManager](reference/classes/sp-pc-error-manager.md).
- [spPCFileStream](reference/classes/sp-pc-file-stream.md).
- [spPCFontManager](reference/classes/sp-pc-font-manager.md).
- [spPCKManager](reference/classes/sp-pck-manager.md).
- [spPlatformSpecificMeshData](reference/classes/sp-platform-specific-mesh-data.md).
- [spPS2App](reference/classes/sp-ps2-app.md).
- [spPS2AsyncFileStreamManager](reference/classes/sp-ps2-async-file-stream-manager.md).
- [spPS2ErrorManager](reference/classes/sp-ps2-error-manager.md).
- [spPS2FileStream](reference/classes/sp-ps2-file-stream.md).
- [spPS2FontManager](reference/classes/sp-ps2-font-manager.md).
- [spPS2Helper](reference/classes/sp-ps2-helper.md).
- [spPS2InputManager](reference/classes/sp-ps2-input-manager.md).
- [spPS2IOPModuleManager](reference/classes/sp-ps2-iop-module-manager.md).
- [spPS2Material](reference/classes/sp-ps2-material.md).
- [spPS2MaterialDataSerializer](reference/classes/sp-ps2-material-data-serializer.md).
- [spPS2Mesh](reference/classes/sp-ps2-mesh.md).
- [spPS2MeshData](reference/classes/sp-ps2-mesh-data.md).
- [spPS2MeshDataSerializer](reference/classes/sp-ps2-mesh-data-serializer.md).
- [spPS2TextureDataSerializer](reference/classes/sp-ps2-texture-data-serializer.md).
- [spRenderable](reference/classes/sp-renderable.md).
- [spRenderableSerializer](reference/classes/sp-renderable-serializer.md).
- [spRenderer](reference/classes/sp-renderer.md).
- [spRenderMesh](reference/classes/sp-render-mesh.md).
- [spRenderNode](reference/classes/sp-render-node.md).
- [spRenderNodeSerializer](reference/classes/sp-render-node-serializer.md).
- [spRenderTarget](reference/classes/sp-render-target.md).
- [spResource](reference/classes/sp-resource.md).
- [spResourceFATSerializer](reference/classes/sp-resource-fat-serializer.md).
- [spResourceManager](reference/classes/sp-resource-manager.md).
- [spSceneGraphOptimizer](reference/classes/sp-scene-graph-optimizer.md).
- [spSerializer](reference/classes/sp-serializer.md).
- [spSerializerHook](reference/classes/sp-serializer-hook.md).
- [spSerializerManager](reference/classes/sp-serializer-manager.md).
- [spSkin / spSkinSerializer](reference/classes/sp-skin.md).
- [spSkyBox](reference/classes/sp-sky-box.md).
- [spSphereBV](reference/classes/sp-sphere-bv.md).
- [spSphereBVSerializer](reference/classes/sp-sphere-bv-serializer.md).
- [spStaticRenderObject](reference/classes/sp-static-render-object.md).
- [spStream](reference/classes/sp-stream.md).
- [spSubscriptionManager](reference/classes/sp-subscription-manager.md).
- [spTaskTimer](reference/classes/sp-task-timer.md).
- [spTemplateInstance](reference/classes/sp-template-instance.md).
- [spTemplateManager](reference/classes/sp-template-manager.md).
- [spTemplateObject](reference/classes/sp-template-object.md).
- [spTemplateSerializer](reference/classes/sp-template-serializer.md).
- [spTextNode](reference/classes/sp-text-node.md).
- [spTextRenderable](reference/classes/sp-text-renderable.md).
- [spTexture](reference/classes/sp-texture.md).
- [spTextureBuffer](reference/classes/sp-texture-buffer.md).
- [spTextureData](reference/classes/sp-texture-data.md).
- [spTextureDataSerializer](reference/classes/sp-texture-data-serializer.md).
- [spTransformTrackEval](reference/classes/sp-transform-track-eval.md).
- [spTransFunctionEvalSerializer](reference/classes/sp-trans-function-eval-serializer.md).
- [spUVController](reference/classes/sp-uv-controller.md).
- [spUVControllerSerializer](reference/classes/sp-uv-controller-serializer.md).
- [spVertexBuffer](reference/classes/sp-vertex-buffer.md).
- [spZone](reference/classes/sp-zone.md).
- [spZonePortal](reference/classes/sp-zone-portal.md).
- [spZonePortalNode](reference/classes/sp-zone-portal-node.md).
- [wxGameFlowState](reference/classes/wx-game-flow-state.md).
- [wxPCApp](reference/classes/wx-pc-app.md).
- [wxPS2App](reference/classes/wx-ps2-app.md).
- [Карточки классов](reference/classes/README.md).
