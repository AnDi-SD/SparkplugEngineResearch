# Цикл ядер tools до 07:30 МСК 10 сентября

Начало: 9 сентября 2026 около23:11 МСК. Состояние: **цикл остановлен**.
Остановка исследования: **10 сентября 2026, 07:28:59 МСК**; далее только
сохранение окончательного отчёта к назначенной границе 07:30.
Граница: 10 сентября07:30 МСК либо доступный лимит; отчёт обновляется по
проверенным блокам, чтобы пережить неожиданную остановку. Исходный checkpoint:
root3d483e0 / Viewer21b43a5. Все ядра пока не готовы.

Приоритет — законченные операции ядер на общих Sparkplug/Winx классах;
интерфейсы вторичны. Ошибки исправляются самостоятельно с доказательством
оригинального поведения для восстановленной логики. Разрешение исправлять
ошибки не отменяет согласование замены игрового алгоритма.

## Организация

- Узкое чтение известных файлов/символов, повторное использование evidence.
- Дополнительный агент только для конкретной независимой задачи; без
  пересекающихся широких аудитов и повторных сборок каждым участником.
- Адресные проверки после изменений, общая проверка связанного блока;
  успешные проверки без новой причины не повторять.
- Один накопительный журнал и компактные доказательства контрактов;
  commits после осмысленных проверенных блоков, без упаковки релиза.
- Около1ГиБ суммарной рабочей памяти; процессы пользователя не останавливать.

## Текущий блок

Source checkpoint: root `7d063d0`, Viewer `b4f3dba`. Общие PRNG и известная часть
renderer cache проверены вместе с шестью связанными suites: 8/8 PASS,
2089 assertions, 19.29 с. Последний native DLL SHA256:
`2B0B7D474DE0EFF16F160DCD59895A4C0ECED6B3169855F49DDA9D08761266EB`.
Новый production код после этого checkpoint не добавляется; завершаются
короткие original probes, документация и сохранение результатов.

## Сохранённые ограничения

Полный список предыдущего цикла: [отчёт](tools-core-cycle-report-2026-09-09-2300.md).
FAT/envelope replacement остаётся предложением, не согласованной реализацией.
Source-less/cached TextureData, large-file memory profile и прежние runtime
границы остаются открытыми. Пользовательские отсрочки LVLcreator не отменены.
О новых нестандартных случаях сообщать сразу и продолжать независимую работу.

## Проверенные результаты

1. Около 23:35 МСК: [Font reader и Static matrix authoring](tool-font-static-shared-core-2026-09-10.md).
   Общий Font заменяет C# parser; два writer-потребителя подключены к actual
   StaticRenderObject serializer. Native 3 suites, Font original/source 3816B,
   Static original/ABI 5 случаев; managed Font 54, Static 50, общая проверка
   Viewer 609 (локальный Samples отсутствует, отдельное menu проверено).
   Viewer/Importer builds без warnings/errors. Все ядра ещё не готовы.
   Checkpoint: root `a9c15df`, Viewer `0c76126`.
2. WinxHairPatcher: проверена операция EXE patch по существующему
   [оригинальному evidence](bloom-hair-system.md). Дублирования игровой
   симуляции/serializer нет; это собственная операция изменения файла.
   Исправлена коллизия backup-имён при двух патчах в одну секунду: добавлен
   уникальный ID операции. Старый код провалил regression; исправленный прошёл
   **38 assertions**, build без warnings/errors. Игровой EXE только читался,
   запись проверялась на временных synthetic файлах. Визуальная проверка всех
   игровых комбинаций остаётся прежним ограничением, не результатом этого блока.
   [Локальные команды и evidence](../../local-data/results/tools-core-cycle-20260910-0730/hair-core/notes.md),
   [manifest](../../local-data/results/tools-core-cycle-20260910-0730/hair-core/manifest.json).
   Checkpoint: root `ca604e7`.
3. [Text metadata inspection](tool-text-shared-inspection-2026-09-10.md):
   удалены отдельные C# Text/inherited Renderable parsers, исправлены
   byte-string/UInt32 wrap, сохранены raw bytes и явная граница layout unavailable.
   TextNode делегирует общей RenderNode metadata-проекции; полного runtime
   TextNode reader это не означает. Native TextInspection/RenderNode прошли;
   managed 59 проверок с 20 Text/TextNode объектов menu; общий Viewer 609,
   Viewer и FormatTests builds без warnings/errors. Документация исправлена.
   Checkpoint: root `57a5218`, Viewer `00407c3`.
4. [Память ResourceGraph](tool-resource-graph-memory-2026-09-10.md): удалена
   полная временная копия входного SMO; один общий read-only host stream.
   На Alfea02 пик working set 44 359 680 → 37 113 856 байт (−16,33%).
   Объекты, узлы, trace и выбранная texture совпали, включая чтение после
   освобождения входа. Время одной пары 0,06326 → 0,07645с: ускорение не заявлено.
   Native BorrowedInput44/FullLoader/ReferenceReadTrace и managed Text59 прошли.
   Ограничения 64МиБ и числа объектов не повышались.
   Checkpoint: root `199f8b2`.
5. [TextureTool header и SanToVmd output](tool-texture-header-vmd-output-2026-09-10.md):
   удалён локальный header parser; подтверждённый OverflowException заменён
   отказом общего reader (19/19). На двух SMO прошла 71 проверка, три PNG
   совпали с исходной сборкой. Новый raw material snapshot не считается
   before/after DTO comparison. Legacy material path остаётся неполным.
   SanToVmd получил отдельный временный файл на операцию: два новых теста
   воспроизвели прежнюю коллизию и прошли после исправления; старые 18 tests прошли.
   TextureTool checkpoint: `e449096`.
   Root checkpoint: `7b6c76e`.
6. [Material reader TextureTool](tool-texture-material-inspection-2026-09-10.md):
   удалён второй parser состояний/pass/layer, общий native snapshot исправляет
   legacy field8 и пустые LTS getters. Старый DLL: 8 failures из70, новый70/70;
   отдельный NULL reassignment guard14/14. Focused71 и три PNG неизменны.
   UI получил только вывод MaterialIssue. Viewer/FormatTests собраны чисто;
   TextureTool GUI собран с двумя прежними CS9057 analyzer/toolchain warnings.
   Checkpoints: Viewer `1dc252a`, TextureTool `46e3cf2`, root `55160c9`.
7. SanToVmd: один полный сценарий Icy/xiid → Miku_Hatsune_Ver2 VMD
   проверен существующим независимым reader: **51 кадр, 51 дорожка, 2601 ключ**,
   три позы и десять сегментов тела. Максимальная ошибка направления
   `1,72e-6` при границе `0,002`. Конверсия и проверка заняли 0,242с,
   пик working set 28 028 928 байт; исходные и staged файлы неизменны.
   [Manifest](../../local-data/results/tools-core-cycle-20260910-0730/vmd-smoke/manifest.json),
   [отчёт](../../local-data/results/tools-core-cycle-20260910-0730/vmd-smoke/report.json).
   Это один адресный acceptance; визуальное воспроизведение MMD не проверялось.
8. [PC TextureData source](tool-pc-texture-source-inspection-2026-09-10.md):
   C# source dispatch заменён наблюдением actual DX reader. Сохраняются
   порядок, repeated fields, выбранное представление, stored/runtime mips
   и opaque skipped fields. Новый cache повторно использует только metadata.
   Native TextureSerialization1219/FullLoader213 PASS; архивный original icebat
   совпал по5460 mip bytes. Managed58: четыре реальные текстуры и десять
   synthetic source-форм; Viewer610. TextureTool71 при экспорте3PNG и2052
   assertions для6замен одного Bloom_body; прежние PNG побайтно совпали.
   Исправлен новый XRGB preview defect на повторных field5 до checkpoint.
   Запись неподтверждённых source форм явно закрыта; FAT остаётся предложением.
9. [PS2 native metadata](tool-ps2-texture-native-inspection-2026-09-10.md):
   удалён C# native header/palette/mip parser. Общий inspector сохраняет raw
   flag, descriptors и wire dataSize; нулевой flag не скрывает данные.
   Native204, real fixture15, managed15 PASS на noisesm из redf03.
   Mip dimensions помечены неизвестными; PS2 source/runtime/swizzle не закрыты.
   Исправлено старое ошибочное описание влияния field6 в class dossier.
   Legacy book остаётся metadata-only: original bool-success без Init
   не является доказательством созданной текстуры; [граница](tool-legacy-texture-source-boundary-2026-09-10.md).
   Checkpoints texture блока: root `e157c03`, Viewer `f4977e7`, TextureTool `e27c054`.
10. [Инспектор ссылок](tool-reference-inspector-cached-texture-2026-09-10.md):
    удалён ручной ID/size parser, исправлен отказ на допустимом NULL4 Font.
    Тот же новый test DLL воспроизвёл отказ со старым Core и прошёл70 checks
    с новым, включая10 sequence cases и20 настоящих Text/TextNode menu.
    Native/UI не менялись. Cached1848 `marble` уточнён отдельно: bytes совпадают
    с прочитанным695, обе копии успешно читает общий PC source inspector.
    Остаток1848 — skipped authoring coverage, не недостающий texture loader.
    Checkpoints: Viewer `2ae9262`, root `9277638`.
11. [Importer texture destination](tool-importer-texture-destination-2026-09-10.md):
    прежняя вставка PC texture в legacy-common book больше не проходит по одному
    лишь metadata decode. Все три creation consumers требуют PC destination,
    удалены legacy template special cases. До исправления6 book write checks
    обнаружили неподтверждённый output; после —42 PC checks,9 source guards
    и4 ранних legacy refusals PASS. Сборка чистая, исходники игры неизменны.
    Конверсия legacy/PS2 контейнера приостановлена как отдельное предложение.
    Checkpoint: root `89fee87`; все34 PC output побайтно равны предыдущему коду.
12. Importer: семь ручных reference-prefix consumers заменены общим reader.
    ModelGraph50, shared/independent static по6 и отказ старого forward-reference
    output прошли. Пять полных replacements побайтно совпали с baseline.
    Host ID patch/owner scope и authoring coverage сохранены; новых игровых
    алгоритмов или UI не добавлено. Первичная карточка TextureData исправлена:
    исторический corpus profile не считается завершённым PC/PS2 runtime.
    Checkpoint: root `ba044a7`.
13. Исправлен host-признак `PixelDataPresent`: он отражает сохранённые mip bytes,
    а не raw `NativeField1C`. Общий reader и raw byte не менялись. Усиленная
    существующая проверка flag0/2 воспроизводит отказ со старым Core; новый
    проходит2052 assertions, экспорт2PNG и6замен Bloom_body за1,18с.
    [Manifest](../../research/tools-core-texture-presence-2026-09-10.json)
    сохраняет hashes кода, baseline и результатов. Новая сборка чистая.

## Следующие проверенные блоки

14. [Общий GPU skinning/picking](tool-gpu-skinning-shared-picking-2026-09-10.md):
    production shader исправлен по original Fixed.rfx; удалены две C# CPU-копии.
    Viewer и LVLcreator используют позиции того же shader. GPU11 arithmetic,
    27 readback/occurrence и14 provider checks прошли. Viewer7 clips/43 poses:
    419 независимых mesh/time comparisons, max0,000554 в прежнем допуске.
    Конечный GUI capture4,48с/264,4МиБ; сборки Viewer/LVL чистые. Потеря
    occurrence в renderer key найдена review и исправлена до checkpoint.
    Viewer checkpoint: `3d90470`.
15. GLB перестал молча нормализовать существенно non-unit веса под видом
    игровой логики. Три ранних refusal сохраняют destination; всего10 checks.
    Три поддерживаемых GLB побайтно равны прежним. Конверсия non-unit
    деформации в GLB отдельно не подтверждена и не выдумывается.
16. [Renderer fallback PC/PS2](tool-renderer-fallback-material-boundary-2026-09-10.md):
    PC protected producer не извлечён, старый capped constructor не повторялся.
    PS2 static chain подтвердил1pass/2StdLayers и второе raw state1=0;
    сохранены15anchors/11ranges, исправлены ошибочные color offsets в досье.
    PC готовность от PS2 результата не повышается; production defaults не менялись.

17. [FBX weights](tool-fbx-skin-weight-boundary-2026-09-10.md): найден аналогичный
    non-unit дефект в SDK eNormalize. Native host теперь явно отказывает,
    сохраняя destination;19 checks прошли за3,075с. Старый bridge воспроизвёл
    ошибку, а для3supported inputs неизменённый SDK importer прочитал
    побайтно одинаковые363616B geometry/skeleton/weights. Общий fixture
    повторно прошёл10GLB checks; normal export не изменён.
    Сборке FBX потребовался разрешённый запуск вне sandbox: MSBuild FileTracker
    получал E_ACCESSDENIED. Первый log и успешный повтор сохранены.

18. [TextureData platform dispatch](tool-texture-platform-dispatch-2026-09-10.md):
    host registry исправлен с DX255 на original DX6/common1.27 managed checks,
    три wholegraph positives и old/new discriminator прошли; Native FullLoader
    иTextureSerialization PASS2,01с. Один original caller probe blooming_flower:
    AL1, объект безInit,32байта чтения родителя,15/15allocations освобождены.
    Это не поддержка legacy whole graph. Старое утверждение о реальном пустом
    TextureData исправлено: pixelsесть, отсутствует source-wrapper; SourceNone
    с действительно пустой секцией пока представлен только synthetic boundary.
    В новом тесте исправлена опечатка hash literal; первый отказ сохранён,
    game reader под тест не менялся.
19. [Importer fitting preview](tool-importer-fitting-gpu-2026-09-10.md): удалён
    третий C# skin evaluator, теперь общий production shader и native spSkin
    palette composition.24 GPU checks PASS;12meshes/3729vertices Icy,
    identity/translation/local rotation, rawweight boundaries иcontext lifetime.
    Три позы используют1context/1upload. Весьrun3,40с/179,9МиБ. Portable
    provider/ownership проверки прошли, Core/GUI сборки чистые. Начальные
    build failures из-за transitive restore и usinglinkedfixture исправлены.
    Viewer test checkpoint: `3706899`.

20. Importer: удалён неиспользуемый `SmoMeshReplacer.Replace` и два его
    write-helper (131 строка старого отдельного packed-vertex writer).
    Tracked поиск root и обоих submodules не нашёл вызывающего кода или
    документированного API-контракта. Действующие `GetBoneSlots`/`FindAncestor`
    сохранены без изменений; Importer.Gui build прошёл без warnings/errors.
    Новые тесты для удаления мёртвого пути не добавлялись. По ошибке выбрана
    Debug-конфигурация вместо уже собранной Release: лишняя native сборка
    заняла 3:14,50. Следующие managed-only проверки используют существующий
    Release bridge и `SkipSparkplugNativeBuild=true` при неизменном native коде.
21. [Индексные границы loader](tool-indexed-loader-boundaries-2026-09-10.md):
    максимальный по размеру Domino04 загрузился целиком (3693 ресурса,
    0,0733 с / 42,55 МиБ); максимальный по object count race_02 остановился
    на незавершённом OcclusionVolume. Лимиты не повышались: новая работа
    нужна в классе, а не в размере буферов. Это два выбранных input, не corpus acceptance.

22. [Occlusion buffer inspection](tool-occlusion-buffer-inspection-2026-09-10.md):
    два ручных C# buffer readers заменены вызовами общих `spIndexBuffer` и
    `spVertexBuffer`. Native owning views прошли26 checks, P/Invoke/SafeHandle16,
    прежний strict inspection profile16 и реальный race_02 metadata10.
    Release-сборки чистые; native game readers не менялись. Это готовая
    операция инспектора, не full Occlusion Init или загрузка уровня.
23. Importer bone-slot lookup использует общий `SmoRenderableCatalog` вместо
    собственного поиска физического Skin-предка. Поздняя смена BaseMesh больше
    не приписывает старому inline mesh чужую палитру; foreign entry отклоняется.
    Три прежние полные замены и14 новых адресных checks прошли: всего64.
    Все три output SMO побайтно равны сохранённым результатам предыдущего
    цикла; SHA проверены по actual files с обеих сторон.
    Release build9,58с, без warnings/errors. Выбор палитры у нескольких
    потребителей и default0 в `ResolveBoneSlot` этим блоком не переопределены.
24. [Original Occlusion producer](tool-occlusion-init-producer-boundary-2026-09-10.md):
    раскрыты обычные PC shape driver13D0460 и compactor450F50. Два вызова
    compactor прошли по2089 instructions; выбор duplicate representative
    меняет raw IB/VB, хотя позиции треугольников равны. Для выбранного
    race_02 четыре позиции различны, эта CRT-зависимость не требуется.
    Actual CPU readers приняли24/60 bytes; shape дошёл до face producer
    470B20→13B4F40→A0D3E0, затем cap до первой face. Full Init не реализован.

25. [Диагностическая ссылка Importer](tool-placement-diagnostic-reference-2026-09-10.md)
    тоже использует общий prefix reader; удалены два ручных UInt32 reads.
    Это только сообщение об ошибке clone verification. Release Core build
    прошёл за18,57с,0 warnings/errors; новый whole-clone acceptance не заявлен.
26. [Защищённая подготовка PC](tool-pc-protected-preparation-2026-09-10.md)
    оказалась конечной: установлены lookup record и стоимость byte-decryption.
    Более короткий Occlusion producer уложился в существующие4M/16s, без
    повышения стандартных лимитов и подмены original instructions.
27. [Original Occlusion одного реального объекта](tool-occlusion-real-object-init-2026-09-10.md):
    shape, полный Init и serializer прошли отдельными fresh probes за9,88–10,77с.
    Reader обработал130/130 байт, сам вызвал Init и удалил временные buffers;
    все tracked allocations трёх проб освобождены. Это не whole-file loading.
28. [Общий face producer и shape driver](tool-occlusion-shape-shared-core-2026-09-10.md)
    перенесены в spOcclusionVolume с существующим общим plane helper.
    Exact plane bits, порядок и связи совпали;149 адресных checks прошли.
    Runtime Init/serializer ещё не перенесены: отсутствует geometry optimizer,
    single-triangle full shape и re-init имеют отдельные ограничения.
29. [Пакет fresh objects](tool-occlusion-reinit-and-batch-2026-09-10.md) подтвердил
    экономию повторной подготовки: одинаковый Init10,0346→0,0490с,
    известный результат одинаковый. Правило bounded cold/warm comparison
    и сохранения capture до teardown добавлено в манифест.
    Original UInt32→UInt16 low16 и zero-origin bounds подтверждены отдельно.
30. [MaterialColorController](tool-pc-material-color-factory-2026-09-10.md):
    original factory/deleting wrapper подтверждены; включена общая resource
    factory, исправлены четыре saved-alpha0→1 по actual PC writes.
    Native235 checks, MaterialSerialization653 и FullLoader213 прошли.
    Реальный lightbeam_projectile.smo теперь загружается:11 объектов/6 nodes,
    Material5 действительно связан с controller6; old DLL отказывала.

31. [MaterialColor в живом графе](tool-material-color-live-graph-2026-09-10.md):
   существующий managed runtime проверен на lightbeam_projectile и Icy,
   по 23 checks. Подтверждены clock consumption, frame cache, force и disposal
   через настоящий загруженный controller. У lightbeam все function types 0:
   изменение цвета или готовый renderer этим acceptance не заявлены.
   Production код после factory не менялся; расширен прежний regression.

32. [PC default-material producer](tool-pc-renderer-default-material-2026-09-10.md)
   выполнен до возврата: 4,780,619 инструкций / 16.32 с, byte-loop exit точно
   совпал с расчётом. Общий spRenderer создаёт настоящий граф из одного pass
   и двух StdLayers, сохраняя неизвестный power. RendererScene 574,
   RendererSubmit 201 и FullLoader 213 checks прошли; CTest 3/3, 4.19 с.
   Новый shared Submit case использует эту фабрику. Device/startup, полный
   multipass backend и original renderer teardown этим блоком не закрыты.

33. [Original geometry helper](tool-occlusion-geometry-helper-2026-09-10.md)
   перенесён частично: настоящий отдельный owner 4604F0/6E76FC, UInt16
   position-only weld, byte comparator и compactor. Исходное имя неизвестно;
   GeometryHelper4604F0 — явно аналитическое обозначение этого объекта.
   230 helper checks, 149 topology и 213 loader checks прошли; CTest 3/3.
   Четыре actual probes используют точную MSVCR71 7.10.7031.4: порядок
   одинаковых вершин отличается от stable fixture. Поэтому у общего caller
   обязательный sort callback без default; Init/tools ещё не подключены.

34. [PC submission cache subset](tool-pc-renderer-submission-cache-init-2026-09-10.md)
   подтверждён полным constructor → state initializer: 768 selector refusals,
   затем пять ordered setters; E_FAIL не мешает обновлению cache. Общий метод
   сохраняет неизвестные и намеренно исключённые caller inputs, а не создаёт
   готовый render context. RendererSubmit теперь 227 checks; общий запуск
   восьми suites прошёл за 19.29 с. Первые две test lambdas требовали explicit
   capture для C++17; исправлены. Затем исправлены только labels evidence:
   E47C — installed material identity, CA0C — palette; алгоритм не менялся.
35. [Один PRNG для функций и частиц](tool-shared-function-particle-random-2026-09-10.md):
   std::mt19937 wrapper заменён alias на уже восстановленный 624-word original
   алгоритм. Сохраняется один SharedRandom singleton, без per-object reseed.
   FunctionEval 290, ColorFunction 118, MaterialColor 235, UVFunction 386,
   ParticleSerialization 46 и три связанных suites прошли. Пять новых assertions
   проверяют перемежение двух потребителей по original words и общему индексу.

36. [Проверка границы material power](tool-material-power-boundary-2026-09-10.md)
   переиспользовала сохранённые captures: все 7 BloomX и 3 Icy selected materials
   уже имеют initialized power; все используют lighting mode 4. Этот guard
   не блокирует их просмотр. Для произвольного unlit пути guard сохранён:
   automatic shader key всё равно читает power. Новый original run или source
   change не потребовались; следующий нужный вход — actual selected lights.

37. [Настоящий looping Particle Init](native-pc-particle-loop-init-pc2-2026-09-10.md)
   выполнен на PC2 bg.smo до естественного возврата: 539 active / 0 free,
   2695 random draws, 2,010,836 инструкций / 6.86 с. Сохранены все records,
   links, world transform настоящего RenderNode и 624 слова PRNG.
   Независимая проверка подтвердила согласованность capture. Это доказательство
   Init, не whole-file загрузки или готового общего producer. Первый fresh run
   остановлен лимитом ещё в texture mip filter; повтор использовал существующий
   character profile, не продолжал остановленный guest.
38. [Подключение света Viewer](tool-viewer-light-cache-boundary-2026-09-10.md)
   локализовано по сохранённому Icy graph: RenderNode3/Skin4 и ambient Light119.
   Не хватает Scene-owned LightManager links и явного activeHierarchy input;
   алгоритмы selection уже общие. Описаны две тонкие операции подключения
   с lifetime и occurrence identity. Source, новый guest и GPU backend не менялись.
39. [Граница Particle 127/128/129](native-pc-particle-loop-init-counts-2026-09-10.md)
   проверена тремя fresh original runs на явно изменённых fixture copies:
   lifetime 1 и rate 127/128/129; pristine файл неизменён. При 127 и 129 все
   частицы активны; при 128 sampler получает 0, active/free остаётся 0/128,
   записи и полное состояние PRNG не меняются. Producer return 128 не означает
   128 созданных частиц. Все вызовы естественно вернулись, cursor 17198;
   независимый audit подтвердил bytes, rings и RNG. Исходное поведение следует
   сохранить при переносе. Другие кратности, world/angle ветки не доказаны;
   production loop path по-прежнему не включён.

## Новые границы и исправления

- [Общая сортировка](tool-shared-sort-dependency-proposal-2026-09-10.md) для
  geometry и Alpha queue остаётся предложением. Оба original caller используют
  один MSVCR71!qsort; найденная версия 7.10.7031.4 не доказывает исторический
  runtime игры. Portable policy и подключение callbacks не реализованы.
- [Particle loop frontier](native-pc-particle-loop-init-frontier-2026-09-10.md):
  первые два original probes bg_particles.smo остановлены на host registration
  и неопределённом reader/input boundary; producer PASS не заявлен. Независимый
  native baseline тоже отказал раньше, на TextureData. Из индекса выбран PC2
  bg.smo: один native run подтвердил first refusal непосредственно на Particle
  Init. Новый пример не подменяет прежние failed captures.
  Последующий natural Init описан в блоке 37. Ещё не покрыты zero-direction,
  worldSpace=false и особые axis-angle ветки; общий loop guard сохранён.
  Блок 39 подтвердил необычную ветку capacity 128. Это actual behavior на
  указанных входах, не разрешение заменять его ожидаемой полной генерацией.
- Прямой повторный Occlusion Init вернулfalse и оставил старые topology entries;
  subsequent cleanup поймал повторный free. Отдельные fresh objects проходят.
  Для single-triangle fresh Init игра возвращаетtrue, но после reverse face
  старые edge.own выходят за текущий faces vector. Эти пути не выдаются за
  поддержанный общий runtime; безопасная подмена allocator capacity не добавлена.
- Новый Occlusion test сначала использовал C++20 bit_cast в C++17 project;
  исправлен на memcpy. Четыре финальных native suites прошли за5,37с.
- Первый MaterialColor destructor probe передал flags аргумент non-deleting
  body4372E0. Это ошибка ABI стенда; свежая проба actual deleting wrapper437610
  прошла, объект освобождён один раз. Два внешних prerequisite allocations
  не выдаются за освобождённые самим контроллером.

- Первый GPU negative test завис из-за непойманного exception нового harness;
  остановлен только собственный процесс. Top-level catch исправлен, повтор
  завершился ожидаемым exit1. Первичная сборка также выявила недостающий
  `System.IO` using; исправлено, обе конечные сборки чистые.
- Для GUI numeric comparison по ошибке использован исторический input до
  исправления PC quaternion rounding8сентября. Архив позволил установить
  известный случай без повторного original probe. На уже сохранённом original
  PRS input все проверки прошли. Failed capture оставлен, допуски неизменны;
  манифест уточнён правилом выбора актуального evidence input.

- Font baseline отсутствует до reader assignment: nullable, не выдуманный ноль.
- Text metadata inspector готов; full runtime Text остаётся незавершённым.
  TextNode использует общий metadata adapter, не полный runtime loader.
- Original null-Font setter и TextNode teardown упёрлись в недостающую среду;
  успешный setter проверен отдельно с настоящим Font. Остатки общей
  инфраструктуры не выдаются за успешное полное освобождение.
- Новые host-дефекты Font (foreign entry и diagnostic pointer) исправлены
  до checkpoint и покрыты адресными проверками.
- Коллизия backup в WinxHairPatcher исправлена и проверена; новых спорных
  изменений игровой логики этот блок не потребовал.
- Три новых texture test ожидания исправлены по существующим контрактам:
  cap одной секции, допустимый нулевой header02 и normalization1→2.
  Production readers не подгонялись под тесты. Новый PC source observer
  не закрывает barelegacy source-wrapper boundary; combined TextureData Corpus profile
  остановлен девятым guard до записи БД.
- Попытка подключить animated textures к одному renderer pass выявила
  несовпадение выбранного примера: actual BloomX Material89 имеет три passes.
  [Проверены пять входов](tool-animated-texture-preview-boundary-2026-09-10.md):
  найденный однопроходный material принадлежит ParticleSystem, остальные
  нуждаются в реальных нескольких passes. Незавершённый patch сохранён
  отдельно и убран из рабочего кода. Нужен подтверждённый multipass backend;
  его blend/depth/default границы исследуются точечно.
  [Начальные renderer states](tool-pc-renderer-startup-states-2026-09-10.md):
  original tail подтвердил пять desired states в двух exact-slice случаях.
  Полный state-init остановлен на protected selector. Последующая адресная
  [проба actual device](tool-pc-device-state-reference-2026-09-10.md) получила
  BLENDOP171=1 и32 начальных texture-stage args за2,8с/~172МиБ. Начальный
  NOTAVAILABLE оказался зависим от restricted token песочницы; идентичный
  разрешённый helper вне неё прошёл. Холодный selector имеет конечный цикл
  более100k инструкций, а не доказанную недостающую среду. Сохранность новых
  states до настройки материала доказана отдельным callgraph audit для
  fresh/default callbacks. Поздние блоки 32 и 34 закрыли PC default material
  и известную часть state-init; остались shader/light inputs и multipass backend.
- Публикация Viewer остановлена автоматической проверкой дважды. Владелец
  `AnDi-SD` и admin/push права подтверждены read-only GitHub API; origin публичный.
  Проверка требует отдельного согласия на 47 commits до `00407c3` в
  `AnDi-SD/SmoViewer:master`. Такой запрос отправлен пользователю; ответа пока
  нет, другие способы публикации не применялись. Локальные commits сохранены.

## Сжатый итог перед остановкой

Практический результат — общие readers/writers/inspectors вместо ряда ручных
копий в приложениях; три потребителя используют общий GPU skinning; реальный
MaterialColor graph теперь загружается. Default material, cache subset,
geometry helper и общий PRNG подготовили следующие операции ядра. Это не
готовность всех семи приложений и не выпуск Viewer.

Проверки выполнялись по затронутым операциям. Последний общий native запуск:
8/8 suites, 2089 assertions. Отдельно прошли реальные MaterialColor clocks,
GPU poses/picking, Importer fitting, texture preview/replacement и VMD export;
точные выборки и границы приведены в соответствующих блоках. Числа разных
версий тестов не складываются в показатель покрытия всего корпуса.

Из измеренных улучшений: пик ResourceGraph на Alfea02 уменьшился на 16,33%,
одинаковая original Occlusion операция после подготовки fresh guest objects
выполнялась 10,0346 → 0,0490 с. Последняя цифра относится только к этой паре,
не ко всему циклу. Для выбора следующих файлов использован готовый индекс.

Главные остатки: подключение света и multipass renderer, перенос Particle
producer, полный Occlusion Init и общая запись контейнера. Sort policy и
FAT/envelope replacement ожидают решения пользователя по правилу общей
реализации. Original lifetime hazards, platform/source boundaries и остальные
ограничения сохранены в [статусе семи ядер](tools-core-migration-status-2026-09-10.md).
Нештатные случаи были; они не скрыты за общим PASS выбранных тестов.

Финальное сохранение: source checkpoint `7d063d0`, основной evidence checkpoint
`8bdfc91`; Viewer `b4f3dba`, TextureTool `5ab82bb`. Дополнительные directed
Particle captures и окончание отчёта сохраняются следующим локальным commit.
Последняя native сборка остаётся той же: после неё новые production изменения
не вносились. Для трёх свежих evidence проверены 45/45 hashes и локальные файлы;
для directed boundary — ещё 13/13 и независимый audit фактических состояний.

Исследовательские пробы и изменения исходников завершены. Финальная проверка:
6 документов, 85 локальных ссылок, ошибок нет; git diff --check прошёл.
Рабочие деревья обоих submodules чистые. Все ядра остаются незавершёнными
в указанном выше объёме; это окончание назначенного цикла, а не всей миграции.
