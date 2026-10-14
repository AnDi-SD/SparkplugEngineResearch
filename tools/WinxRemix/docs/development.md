# Устройство WinxRemix

Launcher → наша PC-прослойка → Remix bridge/renderer с отдельными патчами.

[Контракт рендеринга](rendering-contract.md) описывает нативные источники,
необходимые преобразования и границы классов. Инициализация API выделена в
[RemixApiSession](../winx_remix_api_session.h), владение mesh/material ресурсами —
в [RemixResourceRegistry](../winx_remix_resource_registry.h). Surface/Skin используют
реестр через переходный facade. Обход загруженного реестра света выделен в
[SceneLightSnapshotSource](../winx_scene_light_snapshot.h), порядок подготовки
независимого API-кадра — в [FrameScheduler](../winx_frame_scheduler.h).
[LightPayloadSource](../winx_light_payload_source.h) выделяет вычисление
наблюдаемых light fields из native reader и использует общий PC4B53C0.
[LightWorldObserver](../winx_light_world_observer.h) хранит пассивные before/after
samples исходных light world вызовов; PC entry hook и phase correlation находятся
в [отдельном адаптере](../winx_native_light_world_source.h).
[SceneGeometrySnapshotSource](../winx_scene_geometry_snapshot.h) выделяет
чтение загруженной support membership из
[scene geometry adapter](../winx_scene_geometry.h).
COM/native readers, camera/light API и владение direct light handles остаются
отдельными адаптерами с прежним состоянием proxy. Общий `SceneSnapshot` и
реестр экземпляров ещё не выделены.

В независимом API-пути сначала готовится камера, затем при включённом API-свете
полный поддержанный текущий direct light batch, затем экземпляры. Этот порядок
не требует первого исходного D3D draw. Неполный пакет света не разрешает подачу
геометрии. После COM-чтения render target и `Release` полученной ссылки fence
повторно проверяет scope и состояние операции до синхронизации света.
Перед экземпляром и после внешних API-вызовов проверяются revision, epochs,
scope и light witness. Учёт уже вызванных draw сохраняет запрет повторной
отправки после частичного commit.

| Каталог | Ответственность |
| --- | --- |
| [direct-camera-bridge](../direct-camera-bridge/) | Передача камеры |
| [skinning-bridge](../skinning-bridge/) | Bridge для skinning |
| [renderer-skinning-fix](../renderer-skinning-fix/) | Патч renderer |
| [renderer-alpha-fix](../renderer-alpha-fix/) | Emission прозрачных поверхностей renderer и парный графический стенд |
| [server-instance-audit](../server-instance-audit/) | Диагностика экземпляров |
| [third-party](../third-party/) | Описание внешних зависимостей |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

## Деформация Skin

Оригинальные правила весов, позиции и нормалей описаны в
[контракте Fixed.rfx](../../../docs/engine/animation/skin-deformation.md).
Общий `spFixedShaderSkinning.h` служит численным эталоном восстановленных
выражений. Его проверки входов относятся к аналитическому интерфейсу.

У закреплённого renderer Remix последний вес вычисляется как остаток до
единицы, неположительные вклады пропускаются, нормаль нормализуется по xyz.
Патч [шагов массивов](../renderer-skinning-fix/) этих правил не меняет.
Значит, исправленный transport/stride сам по себе не разрешает передачу
произвольных игровых весов. Нужно проверить ошибку деформации на конкретной
палитре, представление матриц и конечность нормалей. Ограничение Fixed.rfx
в 16 матриц сохраняется даже при большей ёмкости внешнего API.

Искусственные сравнения API, контроль памяти и границы вывода о GPU описаны
в [инструкции стенда Skin](skinning-fixture.md).
Отдельный [compute-стенд](../renderer-skinning-fix/README.md#вычислительное-ядро-на-gpu)
проверяет исходный shader напрямую и не требует запуска полного renderer.

## Снимок загруженной геометрии

[SceneGeometrySnapshotSource](../winx_scene_geometry_snapshot.h) использует
стандартный C++ без Windows/D3D/Remix. Stateless-класс получает reader и
mutation callback на каждый `Capture`, не сохраняет их и не фильтрует
загруженный partition graph по камере или active flags. Сохраняется прежний
обход children/zone roots и проверка known support ABI. Динамическая
membership включает Skin; фильтра rigid eligibility в этом классе нет.
Unknown custom dynamic supports учитываются отдельно и остаются на исходном
пути; неизвестный static/partition support отклоняет снимок.

Результат владеет векторами: `supports` — отсортированный уникальный набор,
`occurrences` — вхождения с node/object/support, source и ordinal исходного
слота. Повторные ненулевые записи сохраняются во вхождениях. Null-слот не
создаёт вхождение и не сдвигает ordinal последующих элементов. Partition payload
имеет собственный source и ordinal `0`. Числовые адреса — значения для текущего
join, без заимствованных native spans, persistent IDs или гарантии lifetime.
Снимок не владеет mesh bytes, world transforms или render resources.

Лимиты сохраняют прежний membership-контракт: 4096 узлов, 4096 уникальных
supports, 4096 элементов отдельного vector, 8 children на узел и 32768 pending
узлов. Provenance ограничен 65536 occurrences. `valid` относится к membership,
`occurrencesComplete` — к полноте provenance: переполнение occurrence-лимита
или отказ дополнительного allocation отзывает только полноту provenance.
Оставшийся префикс occurrences не доказывает полный состав экземпляров.
Отказы обязательных allocations передаются вызывающему адаптеру.

`mutationSerial` берётся до обхода и сравнивается после. Это metadata нашего
адаптера, без гарантии атомарного чтения, поколения native объекта или FreshRt.
PC reader, scope cache и изменение исходной selection остаются в
[winx_scene_geometry.h](../winx_scene_geometry.h). Original selection,
world/resource/material eligibility и независимый API submit по-прежнему
проверяются своими адаптерами. Выделение класса не квалифицирует все сцены,
живую GPU-подачу или полный цикл Toolkit.

Самостоятельный стенд использует собственные буферы и injected readers,
без запуска игры или GPU:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SceneGeometrySnapshot.ps1 -Name fresh-geometry-snapshot-x86 -Platform x86
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SceneGeometrySnapshot.ps1 -Name fresh-geometry-snapshot-x64 -Platform x64
```

## Чтение реестра света

[SceneLightSnapshotSource](../winx_scene_light_snapshot.h) использует только
стандартный C++ и переданные readers. Он проверяет полный загруженный intrusive
реестр до 512 Light-записей, принадлежность сцене, ссылки, count и tail и
возвращает собственные копии байтов. Camera selection, inactive/disabled flags
и dirty bits не исключают записи из снимка. Класс не обновляет payload и не
доказывает свежесть producer потомков; snapshot также не гарантирует атомарность.

[LightPayloadSource](../winx_light_payload_source.h) использует стандартный C++
и общий [spPCLightPayload](../../../Sparkplug/Code/SparkplugDX/spPCLightPayload.h).
Результат владеет cached/candidate массивами и возвращает `Invalid`, `Dirty`,
`CachedMismatch` или `CachedEquivalent`. `worldDirtyMask = flags & 7` и
`lightDataDirty = (flags & 8) != 0` разделяют причины dirty без заявления о
планировании update. Candidate вычислен из наблюдаемых cached world values;
это диагностический результат, который не доказывает свежесть world/controller.
Только `CachedEquivalent` разрешает подмену потребляемых слов; остальные native
слова сохраняются. Интеграция в [native reader](../winx_native_light_source.h)
сохраняет exact-match поведение, флаги включения/A/B и schema журнала.

[LightWorldObserver](../winx_light_world_observer.h) заранее резервирует две
банки по 512 samples, ограничивает batch 4096 вызовами и не выделяет память в
методах. Он сохраняет собственные before/after bytes, inherited flags, число
вызовов и текущий context. Повторный завершённый вызов заменяет latest sample;
вложенный вызов того же Light отзывает observation, разные Light допустимы.
Stale/completed ticket не может отменить новую операцию.

| Lookup | Значение |
| --- | --- |
| `Current` | Контекст квалифицирован, текущие байты совпадают с собственным post-call snapshot |
| `Changed` | Байты отличаются от наблюдавшегося post-call snapshot |
| `NotObserved` | В квалифицированном batch нет sample этого Light; отсутствие producer этим не доказано |
| `Unqualified` | Нет опубликованного batch либо не прошли context/identity проверки |

[PC-адаптер](../winx_native_light_world_source.h) перехватывает entry `4B58D0`
восьмью полными байтами инструкций. Через него проходят primary/direct вызовы
и secondary thunk `4B5960`, который заранее вычитает `B4` из receiver.
Оригинал исполняется один раз с полным аргументом; opaque EAX передаётся без
трактовки как успех. Observer не повторяет producers и не исправляет dirty
bytes. Публикация требует нормального завершения известного plain System Root
и enclosing Manager. Свои epoch/activity guards исключают foreign/standalone
вызовы, не меняя serial, tokens и счётчики прежнего core root witness.

Установка требует существующего startup identity guard, идентичности core hook,
light slot и исходного prefix. Полный SHA256 Debug EXE проверяет launcher.
Startup quiescence остаётся условием установки: восьмибайтовый patch не атомарен.
В конце кадра `ObserveScenes` читает свежий загруженный light registry для сцен
с известными текущими Root tokens и пишет status/type/enabled,
`worldPositionBits`, `cachedRangeBits` и сводные counts. `cachedRangeBits`
копирует исходное слово DX cache `+0x13C` (слово 19 payload PC4B53C0),
без повторного native вычисления. Затем собственная observation отзывается.
`light-world-observer.jsonl` имеет лимит 16 МиБ с возможным выходом последней
строки за границу; core schema 1 сохраняется. Это scoped observation без
гарантии lifetime, атомарности, fresh RT inputs или покрытия всех producers.

Для наблюдавшихся обычных point lights в работающей оригинальной игре
подтверждён вызов этого entry при положении источника за основной камерой.
Их записи в свежем полном реестре того же кадра и завершённого Root/Manager
совпадают с собственными post-call bytes. Для этой ветви отсутствующий world producer
не установлен: адаптер использует предоставляемый игрой реестр. Это ограниченное
подтверждение исходного поведения, без утверждения о всех controllers,
всех сценах, видимом receiver, влиянии света, тенях или результате GPU Remix.

[Анализатор наблюдений](../analyze_light_world_observer.py) отдельно связывает
свет и единственный packet исходной камеры с одинаковыми frame/scene при
стабильных диагностических полях кадра. Он использует исходные row-vector
view/projection и cached world position. В зачёт наблюдавшегося включённого
источника за камерой входят только `Current`, enabled и engine type point `1`
либо spot `2`. Для других observer statuses cached position остаётся
отдельной диагностикой; directional `0` и ambient `3` не получают
классификацию по положению. Отсутствующая или
неоднозначная камера, неактуальный context и нечисловые входы дают
`Unqualified`. `native=false` и `accepted=false` в camera packet системного
D3D9 означают отсутствие API submit; они не исключают наблюдение исходных матриц.
Численная классификация с допуском — собственная диагностика из cached inputs,
без заявления о native x87 или GPU equivalence.

Отдельная диагностика range строит сферу только для связанного с камерой
enabled `Current` point `1` с конечным положительным cached range.
Её максимальная глубина равна `centerViewZ + radius * length(view[2,6,10])`;
норма столбца глубины учитывает масштаб в affine view. Зачёт
`WhollyBehindCameraPlane` требует отрицательной максимальной глубины за
численным допуском; пересечение и касание плоскости выделяются отдельно.
События, пары frame/scene и scene/address keys этой диагностики считаются
отдельно от положения origin. Отсутствующий в старом журнале `cachedRangeBits`
даёт `MissingCachedRange` только на range-этапе и сохраняет прежний origin
учёт. Для spot граница объёма не квалифицирована. Эта cached сфера не
устанавливает RT-форму света, влияние, receiver, тени или fresh RT inputs.

Счётчики анализа разделяют события, пары frame/scene и ключи адресов сцены;
адреса не становятся persistent object IDs или точным числом разных объектов.
Вход ограничен 16 МиБ с последней строкой до 64 КиБ за границей. Raw hash и
число байтов относятся ко всему исходному журналу. Анализатор автоматически
отмечает распознанный незавершённый JSON-префикс только в последнем хвосте;
завершённый JSON без последнего перевода строки учитывается. Повреждённые
завершённые или внутренние строки отклоняются. Папка запуска должна быть закрыта
до анализа. Команда с новым выходным файлом приведена в
[руководстве](guide.md).

Самостоятельные проверки анализатора на собственных JSONL fixtures:

```powershell
python -m unittest discover -s tools/WinxRemix -p test_light_world_observer_analysis.py
```

Они проверяют строгую корреляцию frame/scene, неоднозначную камеру,
диагностические guards, типы света, матрицы, конечность и границы численной
классификации, cached range, масштаб view и касание плоскости камеры,
счётчики, повреждённые строки и сохранение исходного hash.
Это проверка нашего анализатора без запуска игры или GPU.

Семантика типов и world callback описана в
[spLightData](../../../docs/reference/classes/sp-light-data.md) и
[spDXLight](../../../docs/reference/classes/sp-dx-light.md), матрицы — в
[spCamera](../../../docs/reference/classes/sp-camera.md), row-vector transform —
в [Node world](../../../docs/engine/scene/node-world.md).

В [базовом PC-кадре](../../../docs/engine/architecture/engine-frame.md)
Animation и Scene/world выполняются до графической части. Обычный Node world
не отбирает потомков по camera/frustum; range/active selection света для
конкретного объекта — отдельная операция. [Light world](../../../docs/engine/effects/light-world.md)
обновляет DX payload при локальном dirty `1`, но один этот бит не обновляет
per-object light cache. Явный dirty `8` включает
[scene/cache refresh](../../../docs/engine/effects/light-scene-world.md).
Смена raw property без marker оставляет старый payload. Это контракт базовой
цепочки Node → Light → DX; свежесть live custom overrides и controllers
из него не следует, повторное выполнение producers адаптером не включено.

[Адаптер чтения](../winx_scene_read_window.h) использует `ReadProcessMemory`
в собственном процессе. Внутри одного обхода реестра повторный доступ к той же
странице позволяет прочитать её целиком и скопировать соседние Light-записи
из локального буфера. Для разрозненных адресов остаётся чтение точного диапазона.
После возврата из обхода буфер уничтожается: каждая проверка актуальности
заново читает байты, ссылки реестра и принадлежность объектов сцене.

Окно ограничено четырьмя страницами по 4096 байт; при другом размере системной
страницы используется исходное чтение диапазона. Ошибка чтения страницы также
оставляет точное чтение как запасной путь. Проверки границ PC-адреса, размера,
защиты памяти и целостности связного списка сохраняются. Это способ чтения
одного наблюдения, без гарантии атомарного снимка при параллельной записи.

`Test-SceneLights.ps1 -Name fresh-light-check` проверяет собственные буферы
реестра, смену payload и состава, API Create/Draw/Destroy, границы страниц,
запрещённые и guard-страницы. Каждая проверка сохраняет отдельную копию
исходников и результат в новом каталоге; GPU и игровой код не запускаются.

Самостоятельные CPU-стенды обхода и кадрового порядка:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-FramePreparation.ps1 -Name fresh-light-snapshot-x86 -Platform x86 -Suite LightSnapshot
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-FramePreparation.ps1 -Name fresh-frame-scheduler-x86 -Platform x86 -Suite Scheduler
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-FramePreparation.ps1 -Name fresh-light-payload-x86 -Platform x86 -Suite LightPayload
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-FramePreparation.ps1 -Name fresh-light-payload-x64 -Platform x64 -Suite LightPayload
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-FramePreparation.ps1 -Name fresh-light-world-observer-x86 -Platform x86 -Suite LightWorldObserver
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-FramePreparation.ps1 -Name fresh-light-world-observer-x64 -Platform x64 -Suite LightWorldObserver
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-NativeLightSource.ps1 -Name fresh-light-payload-integration
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-NativeLightWorld.ps1 -Name fresh-native-light-world
```

`LightSnapshot` проверяет независимые readers, владение байтами, новый снимок
после изменения данных и повреждённый либо пустой список. `Scheduler` проверяет
порядок callbacks, актуальность контекста, reentry и восстановление состояния
после исключения. Самостоятельные suites используют стандартный C++ и общую
восстановленную арифметику, без Windows/D3D/Remix API; игра и GPU не запускаются.
`LightPayload` проверяет directional/point/spot, masks, owned candidate/cache,
dirty/invalid приоритет, NaN входы и сохранение непотребляемых слов.
`LightWorldObserver` проверяет резервирование, лимиты, owned bytes, повторные
и вложенные вызовы, stale tickets, context changes и исключения predicate.
`Test-NativeLightWorld.ps1` исполняет адаптер и собственные x86 тела: проверяет
primary/secondary вызов, original-once, аргумент/EAX, ret4/ESP/EBX/ESI,
Root/Manager correlation, SEH, foreign activity и scene census. Это CPU-стенд
без оригинальной игры и GPU; игровой/GPU-зачёт observer этим не заявлен.
Интеграционный CPU-стенд `Test-NativeLightSource.ps1` с записывающим API
проверяет прежний converter и API ownership, сохраняя отказ от подмены
несовпадающего либо dirty payload; игра и GPU не запускаются.
Интеграционные CPU-стенды `Test-IndependentSubmit.ps1`, `Test-SelectedSubmit.ps1`,
`Test-SceneLights.ps1` и `Test-LightOwnership.ps1` используют собственные буферы
и записывающий API для проверки первого API-кадра, неполного света, COM/API
reentry и необратимого частичного submit. Они не квалифицируют реальный
renderer, обновление всех producers, ambient/environment или цикл Toolkit.

## Материалы FFP

Общий для привязанных D3D-ресурсов и native-входов
[преобразователь геометрии](../winx_surface_geometry.h) получает наблюдаемый
режим `LIGHTING` вместе с коэффициентами материала. Только явный unlit-вход
разрешает отсутствие нормалей; их замена описана в
[руководстве](guide.md#явные-ffp-материалы).

`Test-MaterialChannels.ps1 -Name fresh-material-check` проходит реальные
Create/Lock/Draw/Reset/Release системного D3D9 и записывает вызовы API Remix.
Сравнение FFP с нормалями и без них проверяет полный raster readback, затем
позиции, RGBA, UV, направление плоских нормалей и все mip-уровни переданных
текстур. Записывающий API проверяет транспорт материала; итоговое освещение
Remix требует отдельной проверки renderer.

### Цвета материалов Remix

У материалов типа Opaque закреплённая версия renderer применяет
`pow(color, 2.2)` к albedo и цвету свечения после операций FFP. Преобразование
выполняется и для текстуры, и для константы; alpha обрабатывается отдельно.
Это правило внешнего renderer, описанное в
[opaque material interaction](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4/src/dxvk/shaders/rtx/concept/surface_material/opaque_surface_material_interaction.slangh)
и [цветовых функциях](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4/src/dxvk/shaders/rtx/utility/color.slangh).

Смешивание свечения рассчитывается в линейном пространстве. Чтобы задать
линейный цвет `E` через `emissiveColorConstant` при единичной интенсивности
и без дополнительной цветовой модуляции, константа должна быть
`pow(E, 1 / 2.2)`. Численные стенды также отключают адаптивную экспозицию,
tone mapping и bloom: эти стадии меняют итоговые экранные значения.

### Диагностика прозрачности

При включённом `rtx.enableStochasticAlphaBlend` reference renderer обрабатывает
часть полупрозрачных поверхностей в отдельном проходе. Обычные Albedo (`23`)
и Vertex Color (`18`) могут показывать находящуюся за ними поверхность.
По этим двум видам нельзя заключать, что потеряны геометрия или вершинный цвет.
Для отдельного прохода доступны значения `rtx.debugView.debugViewIdx`:

| Значение | Содержимое |
| --- | --- |
| `163` | Цвет выбранной прозрачной поверхности; пурпурный означает отсутствие записи |
| `164` | Её нормаль; компоненты выводятся непосредственно |
| `166` | Оставшаяся прозрачность фона после слоёв; белый соответствует единице |

Сопоставляйте эти данные с итоговым кадром и состояниями исходного draw.
Отключение stochastic alpha blend изменяет обработку частичной прозрачности:
поверхность может появиться, а частицы — стать заметными прямоугольниками.
Такое переключение помогает локализовать расхождение, но само по себе
не подтверждает корректность материала.

Если геометрия и alpha присутствуют, а собственное свечение исчезает только
в stochastic-ветви, применяется отдельный
[патч накопления emission](../renderer-alpha-fix/README.md). Он сохраняет
прозрачность и добавляет свет слоёв с учётом их alpha и пропускания предыдущих
слоёв. В инструкции приведены независимый графический эталон и границы проверки.

## Частицы

Следует различать [CPU quads](../../../docs/engine/effects/ballistic-cpu-draw.md)
и [BallisticPFX.vsh](../../../docs/engine/effects/ballistic-shader.md).
Они различаются обработкой возраста, ускорением, размером и цветом.
Готовый CPU vertex/index buffer уже содержит деформацию. Повторное применение
баллистической формулы к таким вершинам сместит геометрию второй раз.
В CPU-ветви с флагом ParticleSystem+AE центр уже берётся из записи, без
интегрирования внутри draw. Для локальных частиц мировой transform добавляет
только position узла; его scale отдельно участвует в billboard basis.
Подмена этого transform полным PRS узла также меняет геометрию.
Общий [CPU update](../../../docs/engine/effects/particle-cpu-update.md)
покрывает существующие записи при выключенном emission и пустом списке
modifiers. Его фиксированная прибавка скорости на каждый внутренний шаг
не эквивалентна интегрированию ускорения по обычному delta.
Игровой менеджер может продвигать только часы эффекта, пропуская обновление
записей. Адаптеру, который будет сам воспроизводить частицы, понадобится
оригинальное решение менеджера о выполнении update; одного кадрового delta
недостаточно. Для передачи уже сформированных CPU quads источником остаются
готовые вершины и установленный оригиналом transform.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
