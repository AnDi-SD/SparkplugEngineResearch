# RTX Remix: закрытый checkpoint ресурсов independent observer, 13 сентября 2026

В закрытом запуске **v8** `play-rtx-20260913-041558-965`, PID **4012**, текущие world/material/resource inputs совпали во всех **1 310 414** выполненных сравнениях. До `Prepare` опубликовано **983 458** подходящих записей `(support, Model)` в сумме по операциям наблюдения; каждая получила подтверждение текущих ресурсов. Это завершает проверку очередного входного контракта. **Нового independent scene submit здесь нет**, существующие native producers и visibility extension продолжают работать.

Отчёт отделён от [checkpoint v4–v6](winx-remix-independent-input-checkpoint-2026-09-13.md). Полные результаты анализаторов, хеши 67 файлов установленного source snapshot, тестовых артефактов и закрытых логов находятся в [машиночитаемом evidence](../../research/winx-remix-independent-resource-checkpoint-2026-09-13.json). Этот аудит ничего не запускал на GPU, не менял игру, DLL, конфигурацию, production или анализаторы.

## Какая версия действительно работала

`install-v8/installation.json` фиксирует установку в **04:15:49 +03:00**; `launch.json` — старт в **04:16:00**, завершённый процесс подтверждён read-only проверкой анализаторов. SHA256 архивного `run/d3d9.dll`, сборки `build-independent-source-v8/d3d9.dll`, installation и launch одинаков:

`54ECF09B2F25A8FFDE72B90ADB08B836CA7C9385C9A3217EEF90AA4324609092`.

**v7** `E72F21DDDC03FEC70034A8C2AB415275DCF8B2951FC3001CBF89A542E042A00A` был собран, но не установлен. Нельзя приписывать ему этот запуск. Source взят исключительно из `local-data/rtx-remix/independent-scene-tests/install-v8/source`; его полный manifest SHA256 — `FDFD2228F4552A867CDDCFB96A2A91AA4C79B85301F495CA6E90DB081611CD5F`. Текущие развивающиеся headers в доказательства не включены.

| Файл frozen snapshot | SHA256 |
|---|---|
| `winx_native_mesh_source.h` | `64AF1953A78ED5BA61CA20D6024BEFA6FF7C48CF2651F48055D1B8EECDA39044` |
| `winx_native_transport_source.h` | `9C4F22F207C117C747CEF5A88C72030CEFB475B338D86FAE07E07214355198A4` |
| `winx_independent_scene_source.h` | `0E1B07FC35F17177B1E0CEEC6076386638A625F6BB358F796897EF0A7B771057` |
| `winx_d3d9_probe.cpp` | `91AE1454753E8D54266D5E2D9E9A57F0B1018FB29D246467E76AB40778CD407A` |
| `winx_surface_submit.h` | `E193563F02627EC45C9CAA73C1FAED4D084C2728CA59519A05BEB74D0137266F` |

Client/server/renderer hashes в installation и launch согласованы. Runtime использует ранее проверенную camera bridge пару; этот этап её не пересобирал.

## Что именно проверяет ресурсный путь

`ResolveCurrentResources` в frozen `winx_native_mesh_source.h:284` получает свежий native mesh и существующие CPU captures вне активного Model/Support draw. Под тем же `guard` он проверяет renderer/device, точные native VB/IB/declaration и COM-адреса, поколения парного CPU capture, покрытие используемых диапазонов, stride, list/strip range, общий native layout, shared owner и повторно прочитанные native заголовки. Скиннинг и неподдержанные layouts остаются вне cohort. Сам reader не вызывает COM, не выдаёт draw-owner credit и ничего не отправляет в Remix.

Собственный `native_transport_source` дополняет это сведениями из **фактически успешных D3D Create**. Наблюдение разрешено при установленных device Release/Reset и resource lifetime/Lock/Unlock hooks. Declaration elements копируются из действительного созданного объекта. Final Release и Reset инвалидируют записи; новых удерживаемых COM references нет. Generation означает очередное наблюдение создания, включая повторное создание interned declaration по прежнему адресу, и не выдаётся за долговечный native object ID.

Frozen `winx_independent_scene_source.h:182` проверяет также полный существующий upload shadow до использования непроверенного CPU capture. Это сравнение помечает собственные capture flags; игровые cache/dirty поля reader не меняет. Перед публикацией `Input` убирает borrowed `Bytes*`/layout pointers, оставляя копии параметров, адреса и поколения. Они принадлежат текущему scope: native/COM адреса остаются borrowed, при сравнении снова проверяются lifecycle, owner и phase. Возвращаемые reader pointers можно использовать только под точным mutex и до reentrant mutation, producer или COM call.

На реальном submit наблюдатель сопоставляет текущие world/material inputs и ресурсные поколения, CPU pair generation, stride/components/range. Повторное полное сравнение всех байтов на каждом draw здесь не заявляется. Следующий экспортёр должен отдельно обеспечить владение собственным payload; этот checkpoint не превращает сохранённые адреса в безопасные долговечные ссылки.

## Закрытый запуск

| Cohort | Логированные кадры | Записи до Prepare за кадр | Фактические сравнения за кадр | Всего сравнений |
|---|---:|---:|---:|---:|
| Алфея, frame 391–1002 | 609 | 922 | 766 | 466 494 |
| Домино, frame 1023–2943 | 1 918 | 220 | 440 | 843 920 |

Пропуски номеров кадров не заполнены предположениями. Отнесение двух числовых cohort к уровням опирается на закрытые переходы, state/capture и наблюдение root, а не на название сцены внутри агрегатного лога. Встречаются и Static, и RenderNode: среди 4 172 sampled compare records соответственно **1 330** и **2 842**.

Итоги independent log:

- `compared == matched == resourceCompared == resourceMatched == 1 310 414`;
- `resourceCandidates == candidates == 983 458`, `resourceRejected == 0`;
- `worldDifferences`, `materialDifferences`, `resourceDifferences`, `uploadDifferences` — **0**;
- dirty candidate operations — **319**: Алфея 99, Домино 220. Все sampled comparisons имеют `computedWorld=false`; это не доказательство live сравнения именно dirty-матриц;
- 4 172 resource samples имеют `resourcesReady=true`, `resourcesMatch=true`. Они дополняют счётчики, но не заменяют их;
- отказов `callbacks`, `phase`, `registry`, `capacity`, `stale` — 0. Другие cohort отсекаются guards: `world=8`, `model=41 665`, `material=2 436`, `pass=135 776`, `texture=24 542`; `missing=75 453` означает отсутствие подходящей независимой записи для последующего owner comparison.

Таким образом, снятие неверного empty-listener guard из v6 позволило наблюдать обычный RenderNode cohort Домино. Его исправление обосновано [контрактом membership/lifecycle listeners](winx-remix-rendernode-listener-contract-2026-09-13.md); восстановленная игровая логика не подгонялась.

Update log: **2 908/2 908** manager batches нормально завершены, **2 898/2 898** запросов phase witness приняты. Опубликовано 8 724 токена, максимум 3 за логированный кадр при capacity64; aborted/overflow/uncompleted/queryRejected histories пусты. `sequence` дошёл до 2 454 028, но это invalidation serial, **не число manager calls**. World-update `SystemRoot` и partition owner root остаются различными сущностями; токен не подтверждает обновление всех custom descendants.

Transport log: **3 277 330/3 277 330** запросов приняты; создано наблюдений VB/IB/declaration **1 855/1 855/12**, retired **1 159/1 159/0**. Максимум живых записей **2 030** при лимите8 192, declarations12 при лимите512; `rejected`, `reused`, `resets` — 0. Баланс Create/retire совпадает с размером registry на каждом логированном кадре. Запросы включают повторные проверки, это не число ресурсов. Последний frame показывает 1 404 записи; финальное непредставленное cleanup не входит в эти counters, поэтому остаток не интерпретируется как утечка. Live Reset/address reuse этим запуском не проверены.

Все три изученных source logs ниже своих cap, имеют полные строки и не менялись во время чтения. Повторный independent analysis совпал с root `independent-closed.json` после нормализации эквивалентного пути run и строковых JSON keys. Runtime logs архивированы и хешированы; чистые счётчики не выданы за отсутствие любых ошибок во всех подсистемах.

## Проверки до запуска

| Сохранённая fixture | Результат | Подтверждённая граница |
|---|---:|---|
| `native-mesh-resource-tests/resources-v1` | CPU **PASS115** | Reader вне native draw, ranges/layout/shared owner, invalid inputs, caller mutex, generation/coverage, отсутствие COM/API/native execution. Его mesh header побайтно равен v8. |
| `native-transport-tests/transport-v1` | CPU **PASS67** | Lifetime registry, copied declaration, retirement, generation, overlap Reset, capacity, allocation failures. Transport header равен v8; actual Lock hook gate добавлялся позднее. |
| `independent-scene-tests/observer-resources-v1` | CPU **PASS60** | World/material, reciprocal listeners и аналитические dirty matrices. Основные headers равны v8. Fixture **не выполняет ResourceInput**; её название не означает отдельного ресурсного integration PASS. |
| `material-channel-tests/native-transport-v1` | **PASS614**, из них transport53 | Настоящий system D3D9 + recording Remix API: Create/Release/Reset, actual slot gates, nonfinal/final refs, stale tokens, fresh Creates. Основные headers и probe побайтно равны v8. Remix bridge и original game code эта fixture не исполняет. |

Счётчики проверок не складываются в число независимых сценариев. У mesh/transport CPU fixtures собственный watchdog30s; у observer outer timeout35s; real D3D fixture использовала ранее существующий собственный watchdog30s. Эти проверки в данном аудите не запускались повторно. Перепроверены хеши всех **55 + 53 + 63** записей трёх существующих evidence inventories; несовпадений нет. Первый ошибочный inventory draft resources-v1 сохранён как invalid: это ошибка сборки перечня путей, не замена результата CPU115.

## Переход уровня и восстановление

F1 sweep действительно загрузил Домино: в `levels/04-Domino01/result.json` есть `readySeconds=9.01` и обычное idle state `current=active=4`, `debugOpen=false`. Затем автоматизация закончилась **`test_interrupted`**, поскольку ожидала отсутствующий `shaders-client/audit.jsonl`. `launch.json` подтверждает, что shader audit в этом запуске был отключён. Исходный результат сохранён и **не преобразован в PASS**.

После этого root отдельно выполнил движение вправо0.6s, сохранил `domino-resource-moved.png` и state, затем закрыл процесс. Root сообщил о просмотре `alfea-resource-start.png` и `domino-resource-moved.png`; автор этого read-only отчёта проверил существование и хеши, но не выполнял дополнительную визуальную оценку. `alfea-resource-state.json` содержит menu state70, поэтому его отдельно нельзя выдавать за подтверждение активного gameplay state Алфеи.

`bridge-config-restored.json` фиксирует восстановление ранее существовавшего bridge config в **04:20:29 +03:00**. Архивированы before/temporary configs; SHA временного bridge.conf совпадает с launch и bridge-config-state. Временный `exposeRemixApi=True` отделён от before. `live.conf` содержит только комментарий; запуск использует обычную opaque alpha normalization и v6+ provenance recovery, а не диагностический A/B переключатель старого этапа. Текущее содержимое активной установки этим отчётом не проверялось и не менялось.

Этот этап даёт проверенный переход от текущего scene graph к текущим world/material/resource inputs для ограниченного ordinary cohort. Independent submission, владение экспортируемыми мешами, все остальные producer paths, полная видимость и корректное физическое освещение остаются отдельной работой. Условная общая оценка **около45%** не пересчитывается из доли совпадений.
