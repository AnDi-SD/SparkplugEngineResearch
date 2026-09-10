# TextureData: platform dispatch и original inline caller

10 сентября 2026. Исправлена и проверена host-регистрация TextureData;
original caller проверен отдельно. FullLoader/TextureSerialization:2/2 PASS;
managed whole-graph regression:27 checks PASS. Это не завершённая поддержка
legacy whole graph.

## Исправление host-регистрации

`tools/SparkplugViewer.Native/ResourceGraph.cpp` ранее регистрировал только
`spDXTextureDataSerializer` с platform mask255 для wire `78EA082B`.
[CP15 actual startup/lookup](native-pc-texture-runtime-mips.md) доказывает
регистрации DX mask6 и common mask1, operation1. Теперь host подключает
существующий DX reader для6 и common reader для1. Остальные регистрации,
reader, guards и lifetime не изменены.

Constructor совместим: общий header factory42DD10 создаёт runtime
DXTexture4AB520; обе существующие реализации используют этот контракт.
Изменена обвязка приложения, а не игровой parser. Native platform2 сохраняет
DX reader; common platform1 получает original42F180 вместо DX42C640.
Это самостоятельно не делает bare legacy payload читаемым.

## Один original caller с настоящим родительским потоком

Исходник `local-data/pc-pristine/Media/SFX/blooming_flower.smo`,110101 байт,
SHA256 `28FF90A536E40FBC84FBD8BF06150A3118D25B1CB7B4AAFB9641D8AFDF19C9A1`.
Platform1, logical origin999. TextureData ID6 `fx_glow`: inline ID/size1231,
object `[1239,17668)`, payload `[1247,17668)`. Bare fields6/0 содержат
64×64 BGRA,16384 байта pixels. Source-wrapper отсутствует.

В одном fresh micro guest actual466B90 читает одну неизменённую directory
запись `[167,193)` под явным host count1; затем actual4678B0 получает
неизменённую ссылку и весь исходный родительский поток. Empty manager/FAT,
RTTI tree, byte stream, name/diagnostic/CRT seams и device AddRef/Release sink
остаются явными inputs стенда. Renderer constructor и GPU не исполняются.

Actual6D1880/6D1940 и4224F0 выбирают common vtable6DDD90. Цепочка:
4678B0 →467670 →42DD10 →4AB520 →42F180 →42EA50. Перед42F180 созданный
DXTexture уже опубликован в entry.object. Source loop пропускает6/0 и
terminator17667. Затем local loop действительно потребляет32 родительских байта:

| Physical range | Наблюдение |
|---|---|
|`[17668,17690)`|field2, header2, payload20, skip|
|`[17690,17699)`|field6, header5, payload4, skip|
|`[17699,17700)`|terminator|

Payload возвращает AL1 в4677C0, caller — тот же опубликованный pointer.
Cursor17700 больше inline end17668; diagnostics отсутствуют. Runtime initialized
byte+24 равен0; cross reader42E100 и texture upload не вызваны. `CCCCCCCC`
в других словах — poison allocator стенда, не игровые defaults.

Main call25983 инструкции/0,149117с; весь guest2,081610с. Caps100000/2с на
вызов,30с child,1worker, arena128KiB/использовано57104 байта. Освобождены все
15 tracked allocations, device AddRef/Release сбалансированы. Working-set peak
процесса не измерялся; arena не выдаётся за всю память. Повтора cap/ctor не было.

## Проверка host-подключения

`--texture-platform-dispatch BLOOMING_FLOWER OUTPUT [BEFORE_DLL]` в
`SmoViewer.FormatTests` использует existing `Document`, `Field`,
`SeedNativeSection` и общий native texture writer. Три tiny synthetic inputs:
common SourceNone+cross; тот же common с unknown field6=2; PC2 SourceNone+native.
Проверяются `SmoLoadedResources`, покрытие полного object extent и whole-graph
C ABI: runtime DXTexture2×2, два mips, точные base pixels. Leaf inspector не
служит oracle этой проверки.

Unknown field6=2 — явная synthetic вариация: original common42F180 пропускает
field6, old host DX255 трактует его как native-platform override и пропускает
cross pixels. Поэтому этот input различает old/new регистрации. Остальные
positive snapshots должны совпадать. Real blooming_flower должен сохранять
`Invalid bounded derived section [inline ID 6, class 1060524470]`.

Root native FullLoader и TextureSerialization:2/2 PASS,2,01с, DLL
`2378E419…`. Managed Run2:27 checks PASS, три whole-graph positive и один
original negative. Unknown-field6 input успешно загружен current DLL, old
`11942803…` отказала с `No restored native DX mip payload`. Plain common и
PC2 дали одинаковые old/current snapshots; blooming_flower сохранил одинаковый
bounded отказ. Все три positive имеют coverage1, runtime2×2/mips2 и точные pixels.

Первый managed run прошёл три current/old synthetic сравнения, затем остановился
из-за65-символьной опечатки в hardcoded original hash; JSON success не записан.
Root исправил только hash literal, не runtime. Исходный failure log сохранён;
повторная сборка чистая,0 warnings/errors. Run2 JSON и hashes привязаны в
manifest. Managed elapsed/working-set peak самим harness не измерены.

## Оставшаяся граница

Original AL1 с неинициализированной текстурой и выходом cursor за inline extent
не доказывает whole-game acceptance или rendering. Host `SectionCursor`,
initialized-payload и exact-inline guards сохранены; optional wrapper, fake Init
и подстановка pixels не добавлены.

В проверенном existing evidence нет original fixture «явный SourceNone field2
и действительно пустая local section». Blooming_flower содержит pixels;
Alfea01 cached1848 ранее независимо подтверждён как непустой. Это разные случаи.
Нового отсутствующего pixel reader или external resolver здесь не установлено.

Следующий узкий target при необходимости legacy runtime: existing original
post-reference loop родительского material reader42F670. Texture caller вернулся
уже в17700 — конце material `[1122,17700)`. Реакция parent reader ещё не проверена;
whole-file scan и повтор выполненной reference-пробы не нужны.

Точные результаты и fingerprints: [manifest](../../research/tools-core-texture-platform-dispatch-block-2026-09-10.json).
Original probe/script/report находятся локально в
`local-data/results/tools-core-cycle-20260910-0730/texture-source-caller/blooming-flower/`;
DLL, SMO и извлечённые данные в Git не входят.
