# PC: общий загрузчик SMO/SAN, FAT и фабрика классов

Checkpoint 6 сентября 2026. Оригинал: `local-data/pc-pristine/WinxClub.exe`,
SHA-256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Историческое слово `pristine` не заменяет fingerprint конкретной сборки.
PS2 не запускался и не получает PC evidence автоматически.

## Целый вызов общего PC loader-а

Неизменённый `bbush.san`: 2554 байта, SHA-256
`706BD0E5C70111BBD7C9524B3A37B1B2D867EC86FDC5A7A7908FAC8BBFCA428E`.
Общий loader `422B50` прошёл целиком:

```text
FFPS header /422260
 -> resource index /466B90 -> RTTI contains /4143F0 ->4423F0
 -> file index /465CD0 ->13BCA90
 -> stream origin += dataOffset
 -> spDXSerializerHook /4AA430, v1C=4AAB80 (platform1: gate skips4AA870)
 -> materialization /422940
    -> serializer lookup /4224F0 ->42C9F0
    -> object header /467550 -> RTTI create /414420 ->41A090
    -> spAnimationSerializer secondary v8 /43ECC0
 -> FAT clear /466760,466870
```

Первый вызов: **79064 инструкции**, второй на том же менеджере: **11949**.
Оба возвращают живой `spAnimation`, потребляют весь файл без ошибок, оставляют
logical origin равным **54** и освобождают временные индексы. Возвращённая
анимация не уничтожается вместе с FAT. Две живые анимации совместно используют
настоящий реестр имён дорожек; удаление одной уменьшает только её ссылки.
В конце освобождены все отслеживаемые native allocations, включая hook,
индексы, сериализатор и созданные менеджеры.

Это целый loader на одном SAN, **не полная загрузка игры**. Начальные контейнеры
SerializerManager/FAT/RTTI заданы явно по потребителям. Native startup/CRT
registration и полные manager constructors не подменены фиктивным успехом и
не считаются пройденными. Allocator, byte-backed PC file stream, named-object
string ownership, диагностика, char_traits и acos — явные границы теста.
Парсинг, фабрики, контейнеры и реестр имён исполняются нативно.
Stream fixture учитывает origin в Seek/Tell, но не в GetSize; короткое ненулевое
чтение успешно, нулевое/EOF — нет. На валидном файле все чтения полные.
Bounded seek не разрешает выходить за предоставленный файл.

## Реестр сериализаторов

`422D90(classID, serializer, platformMask, operationMask)` добавляет node18 в
конец списка. `4224F0 ->42C9F0` выбирает первый совпавший ID с ненулевым
пересечением обеих масок, включая старший bit32. Null serializer у первого
совпадения возвращает ноль, а не запускает поиск следующей записи. `4228A0`
уничтожает каждый уникальный serializer один раз, группируя aliases по первому
появлению. Portable `RegisterForAnalysis` оставляет явный безопасный запрет
null registrations; это отличие host API, не найденный native guard.

## PC FAT отличается от PS2

Allocation PC = **0x58** (`push58` по `13D6E7F` и bounded manager scout до входа
в защищённый ctor), PS2 = **0x64**. Имя C++ helper-а неизвестно; доказан путь
`Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp`.

| PC offset | Наблюдаемая роль |
|---:|---|
| `10` | next save resource ID |
| `14 /20 /2C` | три 12-байтных map: file ID, resource ID, object pointer |
| `38` | ordered file list, 12 байт |
| `44` | неизвестно; не объявлять cursor по симметрии |
| `48 /54` | ordered resource list и current node |

`466B90` читает count, затем `ID /u16-length name /classID /offset /size`.
Создаёт entry24, обнуляет fileID/object, **не инициализирует слово1C** и не
двигает next ID. Заполняет ID map и ordered list; object map остаётся пустым.
Cursor идёт по порядку вставки. Duplicate ID перезаписывает map, но добавляет
ещё один list node: прежняя entry становится утечкой. Это теперь PC runtime
evidence, не только PS2-гипотеза.

`465F00/465F20` — first/next. `466760` уничтожает map-reachable entries в
порядке unsigned ID, очищает контейнеры, ставит next ID=1; объект по entry20
**не удаляется**. Cursor54 может остаться устаревшим; FirstEntry обновляет его.
Duplicate orphan в тесте уничтожается отдельно его настоящим destructor-ом:
это fixture cleanup, не скрытое исправление native clear.

`465CD0 ->13BCA90` потребляет file count и пары `fileID/name`, выделяя entry0C
и строку, но не меняет FAT и не освобождает allocations. Проверены пустой,
несколько/duplicate/пустое имя/trailing и четыре усечённых exact-read входа.
Тесты отказов используют отдельно объявленный strict stream; их нельзя
приписывать Win32 short-read semantics. Portable discarded-file helper
освобождает временные данные, не воспроизводя native утечку.

## RTTI и различие физических / engine типов

PC `414420` ищет class ID в дереве `manager14`, берёт registration из node10,
вызывает factory из record4C с class ID как cdecl аргументом. Нет регистрации
или factory — null. Membership `4143F0 ->4423F0` проверен отдельно, без seam
вместо функции. Allocation20 и head18 подтверждены PC потребителями, не только
сравнением с PS2 allocation24.

Initializer `spAnimation` начинается по **6D1C10**; **6D1C30 — callsite** внутри
него. Статически задаёт class56EE563A, parent75DE50, factory41A090. Engine RTTI:
Animation ->Controller ->SubController ->BaseObject. NamedObject44DE07FD на
анимации даёт false, несмотря на физический named-object prefix. Подмена engine
RTTI физическим наследованием изменила бы cache/name ветви загрузчика.

## Пока статическая разметка, не вся branch coverage

`422940` пропускает уже materialized entries целиком, включая выбор return root.
Для fileID0 пробует ResourceManager cache; miss ведёт в seek/factory/reader.
Указатель сохраняется в entry20 до reader; false AL возвращает null без
локального rollback. Первый вновь обработанный entry задаёт return root.
FileID!=0 вызывает `466490`; происхождение этой ветви ещё не закрыто.

Generic `422B50` отличается от scene-only `422550`: scene требует Node695C0F65,
generic вызывает платформенный hook и обход записей. PC hook slot **1C**, PS2
counterpart slot **24**; номер нельзя переносить между ABI. Mesh-containing DX
hook, fixup/resolver, malformed branch contracts и writer остаются открытыми.

Уточнение последующего materialization checkpoint: bbush header.platform1
не включает PC bit2. Поэтому original4AAB80 был исполнен, но4AA870 в том
whole-load test НЕ запускался; выражение «no-mesh hook» не означает исполнение
всего batch body. Теперь отдельные cache-only cases проверяют bit2 gate и
exact-MeshData selection. Portable whole-file reader уже добавлен поверх
прежних header/FAT/serializer cores; его full bbush output совпал на227 values.
Это не полное DX batch materialization или whole native Save.

## Воспроизводимость

- `probe_pc_loader_registry.py registry`: **78/78**, file-index: **60/60**.
- `probe_pc_fat_runtime.py rtti`: **10/10**, index: **61/61**.
- `probe_pc_san_loader.py bbush.san`: **50/50**, два целых load и lifetime.
- `inspect_pc_loader_runtime.py`: **34/34** fixed-byte anchors, без зависимости
  от ошибочных границ инструкций в старой IDA database.
- `compare_pc_san_loader.py --portable .codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSanReaderTests.exe`:
  **227/227** duration/capacity/имён/owned slots/PRS/tags. Portable сторона
  использует field reader и binding manager, **не полный FAT loader**.
  Parser-derived usedPools исключены: это не снятые runtime поля.

## Отрицательные результаты — не повторять вслепую

- RTTI factory и Animation initializer без готового registry остановлены
  allocator guard на запросеE000 (до этого20/18/20 allocations). Назначение
  большой allocation ещё требует трассы; это не доказанный property pool.
- `6D1C10` с готовым пустым registry достиг100k в protected registration
  (`889BB8`); не возобновлялся и не повторялся.
- Cold whole-load `bflower.san /barrel.san /bw.san` достиг100k соответственно
  по `472923 /41FB28 /13D7892`. Это предел теста, не зависание игры и не
  доказанная ошибка формата. Эти whole-load профили отключены. Отдельное прежнее
  доказательство object reader/PRS на четырёх SAN сохраняется.
- Старые caps Visibility/Occlusion/plane assignment сохраняются. Ни один лимит
  не повышен; original binaries/game/GPU не запускались.

Следующий фронт: directed materialization/resolver -> PC save/FAT offsets ->
Animation writer -> SMO mesh/material dispatch. Это продвижение к
[критериям завершения](pc-smo-san-completion-contract.md), не заявление100%.
