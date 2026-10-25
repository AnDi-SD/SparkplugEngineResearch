# Устройство WinxRemix

Launcher → наша PC-прослойка → Remix bridge/renderer с отдельными патчами.

## Организация файлов

Рабочий код лежит в `src/`: `runtime` связывает D3D9 proxy, API session,
реестр ресурсов и frame scheduler; `native` читает и наблюдает оригинальные
объекты; `scene` готовит membership и independent/selected submit;
`lighting`, `materials`, `textures`, `surface`, `skin`, `shaders` содержат
соответствующие контракты и адаптеры. Перенос файлов сохраняет текущие
границы поведения; состояние proxy ещё требует отдельной декомпозиции.

В `tests/<подсистема>` находятся исходники проверок и их PowerShell/Python
runners. Стенды патчей вынесены в `tests/patches/<комплект>`. Сами patches,
закреплённые manifests и описание остаются в `patches/<комплект>`, а сборка
и применение — в `scripts/patches/<комплект>`. Патчи и исторические manifests
сохраняют исходные байты; записанные в них старые пути описывают свой снимок.

`tests/Copy-SourceSnapshot.ps1` копирует дерево C++ и замыкание существующих
quoted includes с сохранением путей от корня репозитория и сверкой hashes.
Структура frozen inputs — `source/tools/WinxRemix/src`,
`source/tools/WinxRemix/tests` и `source/Sparkplug` для общих заголовков.
Список файлов в snapshot не является списком объектов линковки: runner
по-прежнему явно выбирает нужный test translation unit.

`Build-Probe.ps1 -SourceDirectory` принимает корень инструмента в таком
снимке и читает `.cpp` и `.def` из одного снимка. Для старых frozen inputs
сохранена поддержка прежней плоской структуры; если исторический снимок
не содержал `.def`, используется текущий export definition с явной записью
этого входа в build manifest. Внешние generated includes
проверяются отдельно через `Prepare-Dependencies.ps1 -Offline`.

Пути к анализаторам начинаются с `scripts/analysis`, к подготовке shaders —
с `scripts/shaders`; конфигурация хранится в `config`. Основные root
команды запуска и сборки сохраняют имена.

[Контракт рендеринга](rendering-contract.md) описывает нативные источники,
необходимые преобразования и границы классов. Инициализация API выделена в
[RemixApiSession](../src/runtime/winx_remix_api_session.h), владение mesh/material ресурсами —
в [RemixResourceRegistry](../src/runtime/winx_remix_resource_registry.h). Surface/Skin используют
реестр через переходный facade. Обход загруженного реестра света выделен в
[SceneLightSnapshotSource](../src/scene/winx_scene_light_snapshot.h), порядок подготовки
независимого API-кадра — в [FrameScheduler](../src/runtime/winx_frame_scheduler.h).
[LightPayloadSource](../src/lighting/winx_light_payload_source.h) выделяет вычисление
наблюдаемых light fields из native reader и использует общий PC4B53C0.
[LightWorldObserver](../src/lighting/winx_light_world_observer.h) хранит пассивные before/after
samples исходных light world вызовов; PC entry hook и phase correlation находятся
в [отдельном адаптере](../src/native/winx_native_light_world_source.h).
[SceneGeometrySnapshotSource](../src/scene/winx_scene_geometry_snapshot.h) выделяет
чтение загруженной support membership из
[scene geometry adapter](../src/scene/winx_scene_geometry.h).
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

`DirectOperation` хранит witness основного render target только на время
своей синхронной подачи. При полном покрытии state hooks первый
`GetRenderTarget(0)` и парный `Release` завершаются повторной проверкой
state serial, primary identity, getter dispatch и native camera. Следующие
проверки используют этот witness; изменение любого условия окончательно
отзывает его для текущей операции. Setters, включая неуспешные, state-block
Apply, Reset, Release и reentry сохраняют существующее инвалидирование.
При отсутствии полного hook coverage остаётся прежнее чтение target на
каждом fence. COM-ссылка и результат не переносятся в следующий кадр.

Журнал independent submit содержит `targetQueries`, `targetCacheHits` и
`targetInvalidations`. Поля `lightValidationCalls`,
`lightValidationTimedCalls` и `lightValidationMicroseconds` отдельно измеряют
через QPC полную проверку текущего light witness внутри `GroupCurrent`.
Эти затраты уже входят в `submitMilliseconds`; `prepareMilliseconds` и
`drawMilliseconds` также являются его частями и не прибавляются к нему.
Измерение не пропускает проверки и не меняет их результат.

Модуль [FrameProfile](../src/runtime/winx_frame_profile.h) добавляет наблюдение
оставшейся CPU-подготовки с фиксированным хранилищем. Он включается вместе
с журналом independent submit и пишет только итог кадра, без записей на каждый
вызов. Текущая версия `cpuProfile.schema` — `2`; исходные пять метрик версии 1
сохраняют имена и смысл. `cpuProfile.contexts` содержит `independentPrepare` для `PrepareDirect`,
`independentGroup` для `GroupCurrent` и `other` для остальных вызовов на потоке
кадра: в том числе source scanning, selected/legacy preparation и независимых
проверок вне этих двух функций. `other` нельзя приписывать independent prepare.
В каждом контексте измеряются `compareDrawInput`, `nativeInputCurrent`,
`resourceInput`, `geometryExpand` и `geometryIdentity`. Последний включает
content hash expanded geometry и его привязку к material identity до поиска
mesh cache; он не включает поиск и создание API mesh. `geometryExpand`
охватывает только `ExpandSurfaceGeometry` внутри `CaptureRigidGeometry`
для independent/selected packets; другие legacy-конвертеры этим полем
не покрыты. Контекст определяется внешним динамическим scope, поэтому
reentrant callback внутри preparation/group наследует этот контекст.

В версии 2 добавлены `lightCapture` и `lightValidation`. Первый scope охватывает
каждый `ReadLights` с новым `LocalReadWindow`, включая ранний отказ или
незавершённый из-за исключения capture. Второй начинается после возврата
`ReadLights` в `DirectLightRegistryCurrent` и охватывает полный проход проверки
света, преобразований и API-владельцев. Отказ до capture не создаёт эти scopes.
На каждый вошедший scope приходится одна пара QPC, без таймера на отдельный
источник. `lightCapture` также учитывает вызовы вне currentness-проверки;
сопоставлять его с coarse `lightValidationMicroseconds` следует по контексту
и фактическому набору вызовов, не суммой вложенных медиан.
Измерения включают накладные расходы сбора своих счётчиков; они не вычитаются
из длительности и сами по себе не доказывают ускорение.

`cpuProfile.lightWork` содержит отдельные целочисленные счётчики в тех же трёх
контекстах. `captureValid`, `captureInvalid`, `captureAborted` различают итог
capture; `capturedRecords` — размер сохранённого префикса, `recordReadCalls`
и `recordReadSucceeded` — попытки и успешные чтения полных записей. Успешно
прочитанная запись может быть отвергнута структурной проверкой до включения
в префикс. `virtualQueryCalls` учитывает и неуспешный системный запрос,
`regionCacheHits` — найденную в прежнем восьмислотовом cache запись защиты,
в том числе запрещающую чтение. `repeatedRegionQueries` отмечает повторный
успешный запрос тех же границ региона после вытеснения. Отдельный наблюдатель
хранит не более 64 пар границ внутри одного capture. `regionTrackingOverflows`
считает запросы, чьи новые границы уже нельзя сохранить; при ненулевом значении
повторы являются нижней границей, а не полной статистикой уникальных регионов.
Этот наблюдатель не участвует в разрешении чтения и не переносится в следующий
`LocalReadWindow`, даже при повторном использовании приёмника счётчиков.

`validationAccepted`, `validationRejected`, `validationAborted` различают итог
post-capture scope. `validationRecords` учитывает посещённые записи;
`eligibleRecords` и `convertCalls` — записи, прошедшие active/type/dirty gates
до исходного `Convert`. `witnessComparisons` считает сравнения адресов в
прежнем линейном поиске, `ownerLookups` — обращения к карте API-владельцев.
Ранние отказы сохраняют фактически исполненный префикс. Рабочие значения
накапливаются локально и публикуются при закрытии scope; незакрытый scope
отражается таймером `pendingCalls`. Reset кадра отбрасывает старые локальные
значения вместе с таймером. Сбор данных выключается вместе с профилем.
Новые счётчики не кэшируют результат проверки, native bytes или преобразования
и не меняют порядок page/native/API/owner fences. Анализатор версии 1 должен
отклонять schema 2; для неё нужен явный анализ нового формата с сохранением
исходных записей и всех неполных наблюдений.

Каждая запись содержит `calls`, `timedCalls`, `qpcFailures`, `pendingCalls`,
`ticks` и `microseconds`. `calls` учитывает и ранний отказ. Неуспешный QPC
или уменьшение counter записывается как `qpcFailures`; незавершённый при
границе кадра scope — как `pendingCalls`. Выполняется
`calls = timedCalls + qpcFailures + pendingCalls`. При недоступной частоте
`microseconds` равен `null`; при неполном измерении сумма относится только
к `timedCalls`, а не ко всем вызовам. Измерения inclusive: `resourceInput`
уже входит в `nativeInputCurrent`, а callbacks могут создавать дополнительные
вложенные измерения. Их нельзя складывать друг с другом или с coarse timers.
QPC и сбор счётчиков сами добавляют CPU-затраты; результат — профиль
инструментированной сборки, не доказательство ускорения.

`DirectEndFrame` снимает `frameClock.counter` до записи JSON и сохраняет
`frameClock.frequency`, `counterValid` и `frequencyValid`. При двух пригодных
границах одной частоты длительность равна разности counter, делённой на
frequency; кадровые IDs и `comparison` задают фактический интервал и режим.
Таким образом, новые записи допускают frame/time join для FPS без смешения
full-scene и diagnostic. Непригодную или немонотонную границу нельзя использовать
для FPS. Это clock завершения кадра адаптера, не GPU completion или renderer
screenshot counter. Счётчики и контекст принадлежат потоку; `DirectEndFrame`
сбрасывает только свой поток после вывода, включая случай недоступного журнала.
Таймер, пересёкший reentrant reset, не дописывается в следующий кадр. Порядок
native/currentness fences, callbacks и исходные результаты не меняются.

`SceneLightSnapshotSource::CaptureInto` заполняет переданный контейнер заново:
сначала отзывает прежний результат и очищает records, затем полностью
проверяет header, count, links, уникальность, класс и владельца каждого узла.
Для visited addresses используется ограниченная таблица на стеке;
предельный состав остаётся 512 записей. `DirectOperation` владеет контейнером
для повторных light fences и сохраняет только его выделенную ёмкость.
Каждый fence создаёт новый `LocalReadWindow`, читает текущие bytes и лишь
после полного structural acceptance выполняет прежние conversion/owner checks.
Это не кэш света между fences или кадрами и не атомарный snapshot.

| Каталог | Ответственность |
| --- | --- |
| [direct-camera-bridge](../patches/direct-camera-bridge) | Передача камеры |
| [skinning-bridge](../patches/skinning-bridge) | Bridge для skinning |
| [renderer-skinning-fix](../patches/renderer-skinning-fix) | Патч renderer |
| [renderer-alpha-fix](../patches/renderer-alpha-fix) | Emission прозрачных поверхностей renderer и парный графический стенд |
| [server-instance-audit](../patches/server-instance-audit) | Диагностика экземпляров |
| [third-party](../third-party/) | Описание внешних зависимостей |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

## Деформация Skin

Оригинальные правила весов, позиции и нормалей описаны в
[контракте Fixed.rfx](../../../docs/engine/animation/skin-deformation.md).
Общий `spFixedShaderSkinning.h` служит численным эталоном восстановленных
выражений. Его проверки входов относятся к аналитическому интерфейсу.

У закреплённого renderer Remix последний вес вычисляется как остаток до
единицы, неположительные вклады пропускаются, нормаль нормализуется по xyz.
Патч [шагов массивов](../patches/renderer-skinning-fix) этих правил не меняет.
Значит, исправленный transport/stride сам по себе не разрешает передачу
произвольных игровых весов. Нужно проверить ошибку деформации на конкретной
палитре, представление матриц и конечность нормалей. Ограничение Fixed.rfx
в 16 матриц сохраняется даже при большей ёмкости внешнего API.

Искусственные сравнения API, контроль памяти и границы вывода о GPU описаны
в [инструкции стенда Skin](skinning-fixture.md).
Отдельный [compute-стенд](../patches/renderer-skinning-fix/README.md#вычислительное-ядро-на-gpu)
проверяет исходный shader напрямую и не требует запуска полного renderer.

## Снимок загруженной геометрии

[SceneGeometrySnapshotSource](../src/scene/winx_scene_geometry_snapshot.h) использует
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
[winx_scene_geometry.h](../src/scene/winx_scene_geometry.h). Original selection,
world/resource/material eligibility и независимый API submit по-прежнему
проверяются своими адаптерами. Выделение класса не квалифицирует все сцены,
живую GPU-подачу или полный цикл Toolkit.

Самостоятельный стенд использует собственные буферы и injected readers,
без запуска игры или GPU:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/scene/Test-SceneGeometrySnapshot.ps1 -Name fresh-geometry-snapshot-x86 -Platform x86
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/scene/Test-SceneGeometrySnapshot.ps1 -Name fresh-geometry-snapshot-x64 -Platform x64
```

## Пассивное наблюдение geometry scope

[Geometry snapshot observer](../src/scene/winx_geometry_snapshot_observer.h) подключает
отдельный callback после исходной selection и перед её возможным расширением.
Он работает независимо от прежнего `SceneGeometryObserveOnly` и не включает
SceneGeometry extension или API submit. На кадрах, кратных 300, каждый
наблюдаемый selection scope заново вызывает `ReadRegistry`: это свежий обход
membership для данной selection, без использования первого снимка из scope cache.
Параметры запуска приведены в [руководстве](guide.md).

Каждый scope записывается одним JSONL-объектом с `rows[]`. Для зачёта нужны
одновременно `completed=true`, `fencesStable=true` и `updateValid=true`.
Проверяются текущие scene/camera, original selection, root update witness,
thread, mutation, frame/draw и device epoch. Лимиты — 4096 rows, 4 МиБ на
scope и 16 МиБ на журнал; размер проверяется до записи. Неполный scope или
scope с отозванным fence не получает частичного зачёта; candidate counts
такой записи остаются диагностикой.

World inputs квалифицируются общим
[`ReadWorldInput`](../src/scene/winx_independent_scene_source.h) с отдельным обработчиком
отказов, без изменения счётчиков independent submit. Если требуется, world
matrix вычисляется из наблюдаемых cached PRS в собственном результате.
Mesh-проверка использует
[`ResolveCurrentResources`](../src/native/winx_native_mesh_source.h): нужны полные
uploaded VB и IB, совпадение обоих native CPU-covered диапазонов с uploads
и текущий transport witness. `cpuVertexPartial`/`cpuIndexPartial` сохраняются:
совпадение покрытых диапазонов partial CPU capture не является равенством
всего буфера. Заимствованные resource pointers не выходят из guarded scope;
в журнал попадают собственные значения и результаты сравнений.

Observer не вызывает native producers, COM methods или Remix API и не
устанавливает Bytes verified. Собственный layout cache при чтении может
материализовать layout. `selectedSlots`, уникальные supports, registrations
и model rows считаются отдельно: null/repeat slots, повторные регистрации,
несколько models и диагностические строки без model не становятся одним
числом объектов. Unknown custom dynamic supports не покрыты этим reader.
Числовые адреса, generations и scope IDs служат текущему сопоставлению.
Phase/world/mesh qualification не доказывает FreshRt всех данных, lifetime,
material/draw eligibility или визуальную правильность. View/projection и
`worldSphereBits` — cached inputs; классификация такой сферы относительно
камеры не доказывает положение всей реальной геометрии.

CPU-стенд исполняет адаптер на собственных данных, без оригинальной игры и GPU:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/scene/Test-GeometrySnapshotObserver.ps1 -Name fresh-geometry-observer
```

## Собственный пакет жёсткой геометрии

[RigidGeometryPacketBuilder](../src/surface/winx_rigid_geometry_packet.h) отделяет результат
преобразования от native/COM ресурсов и Remix SDK. Класс использует стандартный
C++, получает converter и predicate актуальности на один `Capture` и возвращает
собственные expanded triangle vertices, коэффициенты albedo/emission, признаки
vertex color/alpha, числовую provenance и материал. Converter получает четыре
ссылки: vertices, plan, provenance, material. Позиции, нормали, UV, world matrix и
коэффициенты проверяются на конечность; коэффициенты ограничены `[0,1]`.
Пакет ограничен 98304 вершинами, каждые три образуют треугольник.

Преобразование в `PrepareNativePacket` использует существующий
[ExpandSurfaceGeometry](../src/surface/winx_surface_geometry.h). Заимствованные native bytes
и layout потребляются до внешних COM/API-вызовов; backend отдельно перепаковывает
собственные вершины в SDK structs. Predicate проверяется до и после конвертации.
PC-адаптер сохраняет world/material/resource qualification, scope fences и
повторную проверку при потреблении пакета. Сам пакет не разрешает submit.

Provenance копирует поколения, scope/frame, ordinal и количество вхождений,
world matrix, `cpuVertexPartial`/`cpuIndexPartial` и half-open coverage intervals
в координатах исходных буферов. Для каждого буфера допускается до 4096 intervals.
Builder не проверяет их покрытие и не сравнивает bytes с D3D upload: эти условия
остаются в native mesh adapter. Partial CPU capture и совпадение покрытых
диапазонов не доказывают равенство всего исходного буфера. Числовые адреса и
generations не становятся persistent IDs, lifetime tokens или доказательством
FreshRt.

Material содержит поддержанную `MaterialRecipeSource::Recipe`, числовые
device/texture identities, lifecycle/content generations, размеры/формат/mip count,
а также отдельные equation/instance/bootstrap values. Recipe и transport witness
получаются заново после первой проверки актуальности. Backend восстанавливает
SDK sampler/instance из собственных значений пакета; адрес текстуры заново
квалифицируется адаптером. Native recipe не смешивается с собственной unlit policy.
Этот пакет не содержит текстурных pixels или общего снимка сцены.
Контроллеры, native lifetime и API resource ownership остаются у адаптеров.
CPU-проверки владения, границ и failures не квалифицируют оригинальную игру,
реальный GPU, все материалы или цикл capture/replacement в Toolkit.

Guarded `ChannelTextureHashCurrent` проверяет актуальность до публикации хеша,
`WriteTextureSourceCurrent` — после чтения mip и до публикации DDS.
`SurfaceChannelMaterialCurrent` проверяет её до API Create. Смена content generation
внутри чтения не должна оставлять устаревший hash/asset для следующей операции.
Эти проверки дополняют fences перед DrawInstance и учёт частичного commit.
Legacy-вызовы без собственного mip snapshot сохраняют синхронное чтение под
действующим qualified borrow. Проверки до/после такого чтения не продлевают
жизнь COM-объекта; их lifetime-контракт не расширен. Independent/selected путь
использует отдельный захват с временной COM-ссылкой, описанный ниже.

Самостоятельный стенд сохраняет граф локальных зависимостей и JSON-результат в новом
каталоге, использует `/WX` и завершает собственный fixture при превышении
10 секунд. Перед сборкой требуется 512 МиБ доступной физической памяти:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/surface/Test-RigidGeometryPacket.ps1 -Name fresh-rigid-geometry-packet-x86 -Platform x86
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/surface/Test-RigidGeometryPacket.ps1 -Name fresh-rigid-geometry-packet-x64 -Platform x64
```

## Собственный снимок текстурных mip

[TextureMipSnapshotSource](../src/textures/winx_texture_mip_snapshot.h) — stateless-класс
стандартного C++ без COM, D3D, файлов и Remix API. `Capture` получает копию
описания width/height/format/levels, reader и current predicate. Каждый mip
владеет плотно упакованными raw DWORD-пикселями без padding исходных строк.
Поддержаны ARGB/XRGB, размер до 2048 по каждой оси и естественная mip-цепочка;
бюджет вместе с DDS-заголовком — не более `6*1024*1024` DWORD. Выходные векторы
выделяются до reader. `IsValid` повторно проверяет форму изменяемого снимка,
но не разрешает native чтение или draw. Ошибка возвращает полностью пустой
invalid snapshot; исключения передаются вызывающему коду.

[Адаптер захвата](../src/textures/winx_texture_mip_capture.h) проверяет managed2D/pool/usage,
удерживает временный `AddRef` и закрывает каждый успешный readonly `LockRect`
одним `UnlockRect`, включая false/исключение predicate. Padding пропускается;
до копирования проверяется представимость диапазона строк. Временная ссылка
освобождается до последнего current fence. Если игра отпустила последнюю свою
ссылку, окончательный Release может удалить transport record; такой захват
отвергается до cache/file/API publication. Snapshot после возврата не содержит
native указателей и не требует живой текстуры для hash или DDS.

Independent/selected submit захватывает pixels лениво: при пропущенном hash
снимок передаётся дальше, при известном hash и отсутствующем DDS создаётся один
снимок для всех коэффициентов материала. Hash сохраняет прежний layout и raw
порядок всех mip, включая неиспользуемый XRGB alpha. DDS использует прежние
header/Tint/UNORM8 equations, нормализуя XRGB alpha до 255 только в выходной
копии. Оба DDS строятся из тех же pixels, что и hash. Размеры/формат/число mip
сверяются с packet provenance. Тёплые hash/material caches не читают pixels;
при несовпадении повторно прочитанного hash запись кэша отзывается.

Существующий DDS по прежнему принимается по наличию файла. Содержимое такого
файла этим путём заново не проверяется. Native instance lifetime, произвольные
texture formats/controllers и capture/replacement identity остаются отдельными
контрактами. CPU-стенд не подтверждает изображение игры или цикл Toolkit.

Самостоятельный стенд сохраняет четыре относительных входных файла, требует
512 МиБ доступной памяти, собирается с `/W4 /WX` и ограничивает собственный
процесс десятью секундами:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/textures/Test-TextureMipSnapshot.ps1 -Name fresh-texture-mip-x86 -Platform x86
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/textures/Test-TextureMipSnapshot.ps1 -Name fresh-texture-mip-x64 -Platform x64
```

## Содержимое и API-идентичность

[ResourceIdentitySource](../src/runtime/winx_resource_identity.h) использует стандартный C++
и переданный hash writer. Протокол записывает фиксированные 32/64-bit значения
в little-endian; float — конечные IEEE-754 binary32 bits с сохранением signed
zero. Native addresses, SDK padding, world/instance transforms, frame, scope и
resource generations не являются содержимым. Digest 0 означает недоступный ID;
64-битный digest не обещает математического отсутствия коллизий.

`Material` сохраняет прежний channel descriptor: domain
`winx-independent-ffp-v1` без NUL, texture hash, U/V/mag как uint64 и albedo3 /
emission3 как float bits. На Windows x86/x64 сохраняются прежние material hash
и имена DDS. Vertex color/alpha policy уже представлена выходными цветами
вершин; blend/cull остаются instance inputs.

`GeometryExpanded` и `GeometryIndexed` записывают domain
`winx-geometry-content-v1`, количества вершин/индексов, position3, normal3, UV2,
color32 и порядок uint32 indices. Expanded использует последовательные индексы;
та же indexed representation получает тот же ID. Другую раскладку одинаковой
поверхности класс не нормализует. Поддержаны ограниченные triangle lists до
98304 вершин и индексов. `BoundMesh` отдельно связывает geometry ID со стабильным
material ID под domain `winx-bound-mesh-v1`.

Canonical geometry payload по-прежнему собирается в фиксированный буфер
4096 байт без whole-mesh allocation. На Windows x86/x64 одна целая вершина
упаковывается раздельными копиями position12 / normal12 / UV8 / color4,
если объявленные поля — C-массивы `float` достаточной длины и `uint32_t` color.
Общая раскладка Vertex не требуется: leading/interleaved/trailing и SDK padding
не копируется. Для volatile источника, иных field/container типов и платформ
остаётся прежняя word serialization. Если вершина пересекает край буфера,
она также проходит прежний scalar путь: размеры и момент каждого provider
`Update` остаются прежними, без чтения последующих полей через callback/throw
границу. Finite/count/index prevalidation завершается до `Hash::Reset`;
недопустимый вход не изменяет hash state. Signed zero сохраняется побитово,
`Material` и `BoundMesh` не меняют протокол. Hash exceptions продолжают
распространяться без дополнительного flush во время unwinding.

Проверка ResourceIdentity сопоставляет полный byte stream и Update chunks
с независимым scalar oracle на границах буфера, разных layouts/padding,
float edge cases и максимальном составе. Закреплённый XXH3 дополнительно
сверяет streaming IDs с one-shot hash независимого canonical потока;
его header заморожен и проверен по dependency SHA256 в test snapshot.
Проверки provider callbacks/throws сохраняют точку чтения split-вершины
и отсутствие hash mutation при позднем malformed компоненте. Сохранение
ID в CPU fixture не заменяет игровую проверку производительности и изображения.

Ordinary independent/selected путь и legacy expanded Surface используют общий
`SurfaceRigidGeometryResourceStable`; `SurfaceGeometryResource` — тонкая обёртка
того же пути, без собственного hash/cache алгоритма.
`MaterialIdentityOwned` получает material ID из пригодной записи registry под
действующей операцией владения, затем backend передаёт BoundMesh ID в API.
Один mesh resource имеет неизменяемую привязку материала; pure geometry ID
сам по себе не годится как общий API handle для разных материалов. Занятый
ключ с другим live material handle возвращает отказ, без повторного Create.
Quarantine, eviction, порядок mesh → material retirement и поздние current
fences сохраняются. Indexed CPU Skin и GPU Skin используют прежние ключи:
идентичность деформируемого ресурса требует отдельного контракта.

`Test-ResourceOwnership.ps1` запускает recording проверки владения, давления на
cache и адаптера content IDs. Они включают совместное использование rigid
ресурса legacy/ordinary путями, смену material handles после retirement,
изменение содержимого и SDK padding, а также отказ для устаревшего либо
quarantined владельца. Эти CPU-проверки не запускают игру, GPU или Toolkit.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-ResourceOwnership.ps1 -Name fresh-rigid-resource-ownership
```

В [закреплённом renderer](rendering-contract.md#идентичность-ресурса-и-экземпляра)
API hash является handle внешней таблицы. Geometry component hashes задаются
процессным счётчиком; capturer использует их вместе с renderer material hash.
External submit обходит обычный mesh/material replacement lookup.
[Существующие USD patches](../patches/renderer-usd-fix/README.md) исправляют захват
внешнего материала и экспорт, но эту границу identity/replacement не меняют.
Детерминированные CPU/API ключи не квалифицируют Toolkit replacement ID.

[Отдельный renderer patch](../patches/renderer-resource-identity/README.md) добавляет
явную пару asset ID и `rtx.enableExternalAssetIdentity=false`. Допущена одна
rigid Opaque поверхность без particles/GPU instancing. ID именуют capture и
задают mesh/material mod lookup; material content hash BLAS и external instance
matching сохраняются отдельно. Поддержаны Opaque material mod, одна mesh /
includeOriginal запись либо пустая замена для скрытия; неподдержанные формы
возвращают исходную geometry. Capture сохраняет исходный внешний материал.
Стенды компилируют применённый std-only header и извлечённые настоящие функции
`submitExternalDraw`, `newInstance`, `captureMaterial` с recording collaborators.
Они проверяют выбор ключей и ветвей. На production renderer отдельно проверены
однокадровый GPU capture без текстур, чтение USD и стабильность ID при изменении
порядка создания ресурсов. Применение mod, история движения, Toolkit и исходная
игра остаются отдельными проверками. Launcher настройку не включает.
Оба явных capture ID принадлежат `RtInstance`. Общий BLAS сохраняет input первого
draw кадра и не определяет admission соседних экземпляров. Нулевой mesh asset ID
выбирает прежний внутренний BLAS hash, сохраняя default naming. Версия renderer
patch v2 и recording regression проверяют оба порядка смешанных draw/capture.

Самостоятельный CPU-стенд проверяет byte protocol на x86/x64, границы, finite
данные, signed zero и независимость от адресов/padding. Он сохраняет четыре
исходных файла, требует 512 МиБ доступной памяти и использует `/W4 /WX`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-ResourceIdentity.ps1 -Name fresh-resource-identity-x86 -Platform x86
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-ResourceIdentity.ps1 -Name fresh-resource-identity-x64 -Platform x64
```

## Чтение реестра света

[SceneLightSnapshotSource](../src/scene/winx_scene_light_snapshot.h) использует только
стандартный C++ и переданные readers. Он проверяет полный загруженный intrusive
реестр до 512 Light-записей, принадлежность сцене, ссылки, count и tail и
возвращает собственные копии байтов. Camera selection, inactive/disabled flags
и dirty bits не исключают записи из снимка. Класс не обновляет payload и не
доказывает свежесть producer потомков; snapshot также не гарантирует атомарность.

[LightPayloadSource](../src/lighting/winx_light_payload_source.h) использует стандартный C++
и общий [spPCLightPayload](../../../Sparkplug/Code/SparkplugDX/spPCLightPayload.h).
Результат владеет cached/candidate массивами и возвращает `Invalid`, `Dirty`,
`CachedMismatch` или `CachedEquivalent`. `worldDirtyMask = flags & 7` и
`lightDataDirty = (flags & 8) != 0` разделяют причины dirty без заявления о
планировании update. Candidate вычислен из наблюдаемых cached world values;
это диагностический результат, который не доказывает свежесть world/controller.
Только `CachedEquivalent` разрешает подмену потребляемых слов; остальные native
слова сохраняются. Интеграция в [native reader](../src/native/winx_native_light_source.h)
сохраняет exact-match поведение, флаги включения/A/B и schema журнала.

[LightWorldObserver](../src/lighting/winx_light_world_observer.h) заранее резервирует две
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

[PC-адаптер](../src/native/winx_native_light_world_source.h) перехватывает entry `4B58D0`
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

[Анализатор наблюдений](../scripts/analysis/analyze_light_world_observer.py) отдельно связывает
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

[Адаптер чтения](../src/scene/winx_scene_read_window.h) сохраняет общий
`ReadProcessMemory` и прежнее окно из четырёх страниц как отдельные способы
чтения. Обход Light-реестра использует `LocalReadWindow`: `VirtualQuery`
проверяет committed-регионы с разрешённым чтением, после чего точный диапазон
копируется в собственном процессе под SEH для access violation и in-page error.
Guard, no-access и execute-only страницы отвергаются без обращения к их байтам.
Границы PC-адресов и предел одного чтения в 16 КиБ сохраняются.

Локальное окно хранит защиту не более восьми регионов в течение одного
синхронного обхода. Байты объектов не кэшируются; каждый запрос копирует их
заново. Заголовки и записи проходят через одно окно, которое уничтожается
до COM, Remix API или игрового вызова. Следующая проверка актуальности получает
новое окно и заново проверяет ссылки реестра и принадлежность объектов сцене.

Защита и отображение памяти должны оставаться неизменными до окончания обхода.
Проверка `VirtualQuery` и последующее копирование не являются атомарной операцией:
при параллельном `VirtualProtect`, освобождении или повторном отображении
памяти эквивалентность `ReadProcessMemory` не гарантируется. Снимок также не
обещает атомарности при параллельной записи payload. Ошибка чтения не публикует
повреждённый заголовок или запись как действительные; уже проверенный префикс
списка сохраняется с `valid=false`.

`Test-SceneLights.ps1 -Name fresh-light-check` проверяет собственные буферы
реестра, смену payload и состава, API Create/Draw/Destroy, границы страниц,
запрещённые и guard-страницы. Каждая проверка сохраняет отдельную копию
исходников и результат в новом каталоге; GPU и игровой код не запускаются.

Самостоятельные CPU-стенды обхода и кадрового порядка:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-FramePreparation.ps1 -Name fresh-light-snapshot-x86 -Platform x86 -Suite LightSnapshot
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-FramePreparation.ps1 -Name fresh-frame-scheduler-x86 -Platform x86 -Suite Scheduler
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-FramePreparation.ps1 -Name fresh-light-payload-x86 -Platform x86 -Suite LightPayload
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-FramePreparation.ps1 -Name fresh-light-payload-x64 -Platform x64 -Suite LightPayload
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-FramePreparation.ps1 -Name fresh-light-world-observer-x86 -Platform x86 -Suite LightWorldObserver
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/runtime/Test-FramePreparation.ps1 -Name fresh-light-world-observer-x64 -Platform x64 -Suite LightWorldObserver
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/native/Test-NativeLightSource.ps1 -Name fresh-light-payload-integration
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/native/Test-NativeLightWorld.ps1 -Name fresh-native-light-world
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

[MaterialRecipeSource](../src/materials/winx_material_recipe_source.h) выделяет чтение
обычного материала в stateless-класс стандартного C++. На один `Capture`
передаются reader и request с числовыми адресами renderer/material/device
и outer override. Результат владеет raw texture/material words, mapped stage,
sampler и material states вместе с маской присутствия. Числовые адреса цепочки
material → pass → layer → texture holder → native texture служат provenance
текущей операции; они не продлевают время жизни исходных объектов.

Класс принимает только известные ABI, один pass и один обычный слой, режим
материала `2`, без color/UV/animation controllers и static UV transform.
Он использует общие восстановленные
[texture mapping](../../../Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h)
и [material mapping](../../../Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h).
Значение final blend pass записывается только в собственную копию material
words. Ошибка чтения или неподдержанный вход возвращает пустой invalid recipe
с причиной отказа; исходные записи остаются неизменными.

[PC-адаптер](../src/scene/winx_independent_scene_source.h) отдельно выполняет более узкий
`Decode` texture/alpha operations, задаёт bootstrap stage arguments и сохраняет
draw qualification. Даже valid recipe не доказывает фактическую привязку
текстуры. В оригинале debug singleton создаётся лениво; его ненулевой byte
`1F` принудительно передаёт NULL при texture bind, независимо от identity cache.
Адаптер не вызывает этот constructor: он принимает только доступный singleton
с нулевым флагом, а NULL или ошибка чтения оставляет исходный путь.

Native/COM binding, текущие generation и texture content witnesses, scene/update
fences и повторные проверки перед отправкой остаются вне класса. Изменение
debug flag после capture отзывает разрешение до API draw; после уже принятого
экземпляра учёт частичного commit предотвращает повтор через исходный draw.
Recipe не подтверждает полное покрытие материалов игры, их GPU-вид или цикл
Toolkit. Самостоятельный CPU-стенд использует собственные буферы и reader,
сохраняет полный граф локальных include и не запускает игру или GPU:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/materials/Test-MaterialRecipeSource.ps1 -Name fresh-material-recipe-x86 -Platform x86
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/materials/Test-MaterialRecipeSource.ps1 -Name fresh-material-recipe-x64 -Platform x64
```

Общий для привязанных D3D-ресурсов и native-входов
[преобразователь геометрии](../src/surface/winx_surface_geometry.h) получает наблюдаемый
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
[патч накопления emission](../patches/renderer-alpha-fix/README.md). Он сохраняет
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
