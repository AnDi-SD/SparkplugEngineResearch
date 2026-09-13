# Winx Remix: закрытая проверка Skin palette и света — 13 сентября 2026

В завершённом запуске `play-rtx-20260913-082523-549` все **167 961** допущенное
сравнение Skin palette совпало побитно с игрой: **2 687 376** сравнений матриц костей,
ноль отличающихся float words и ноль превышений допуска. Это проверка текущих
исходных данных; **weighted geometry в Remix не подавалась**. Отказы и вызовы
без mesh не исключены из учёта.

Игра PID **488** работала с 08:25:24.933 до 08:45:04 МСК; server PID **15220**
завершил обработку очереди и cleanup в 08:45:00.938. Переходы **27 → 4 → 27**
прошли штатно. Новый анализ выполнен только после закрытия обоих PID.
[Manifest](../../research/winx-remix-native-skin-source-checkpoint-2026-09-13.json)
ссылается на **185 файлов** evidence, frozen inputs, исходные A/B и журналы.

## Что проверено

Собственный observer читает палитру только в mesh call исходного `spSkin::Render`,
RA `46A367`, под текущим owner/scene scope. Общие `Affine` и
`Multiply4ForAnalysis` вычисляют `inverseBind × cached bone world` для plain Node;
producer повторно не вызывается, игровые dirty fields не очищаются.
Подробная ABI/lifetime граница — в
[контракте observer](winx-remix-native-skin-observer-contract-2026-09-13.md).

| Счётчик завершённых кадров | Результат |
|---|---:|
| Исходные Skin calls | 232 489 |
| AL-false / без собственного mesh call | 3 362 / 3 362 |
| Попытки palette observer | 229 127 |
| Совпало / mismatch | 167 961 / 0 |
| Отклонено по scene guard | 61 166 |
| Прочие причины отказа | 0 |
| Максимальная абсолютная / относительная ошибка | 0 / 0 |

Баланс точный: `229127 = 167961 + 61166`. В этом запуске также
`232489 = 229127 + 3362`, однако анализатор не превращает это наблюдаемое равенство
в общее правило: Skin scope и mesh attempts — разные счётчики. AL-false не
означает автоматически сбой рендера; исходный pre может поставить Skin в alpha
queue и выйти до собственного mesh. Конкретная причина каждого такого выхода
этим observer не трассируется.

Выборка менялась при переходах:

| Подтверждённый checkpoint | Skin calls | Matched | Scene reject | AL-false | Матриц |
|---|---:|---:|---:|---:|---:|
| Начальная Алфея, frame664 | 18 | 11 | 7 | 0 | 176 |
| Домино после перехода, frame1408 | 19 | 12 | 7 | 0 | 192 |
| Возврат в Алфею, frame5856 | 39 | 31 | 7 | 1 | 496 |

Возврат не подменяется первым набором 11/18. Scene rejection означает отказ
составного guard текущей сцены/камеры; журнал не доказывает, что каждый такой
вызов принадлежал UI. Из **543** подробных compare samples все имеют 16 костей,
`meshWeights=4`, `weightHint=0`. Значения hint и числа компонентов различаются;
этот журнал не доказывает значения vertex weights/indices. Адреса samples не
являются постоянными идентификаторами объектов.

## Свет и сопутствующая регрессия

Строгий light analyzer — **PASS**: `88484 attempts = 88484 matched`,
`used=87118`, mismatch/dirty/invalid=0. Нарушений `comparison=true ⇒ used=0`
нет. Исправленный порядок `EndFrame → ApplyLiveConfig` относится именно к
установленному frozen source этого запуска. Прежний
[FAIL в 073622](winx-remix-native-light-source-checkpoint-2026-09-13.md)
с неправильными подписями двух граничных кадров остаётся историческим результатом.

| A/B/A2 | Frames | Native computed fields used |
|---|---|---|
| Алфея v1 | 664 / 723 / 780 | 20 / 0 / 20 |
| Домино v2 | 4160 / 4253 / 4349 | 1 / 0 / 1 |

Все шесть случаев имеют точную неизменную camera и верный уровень. Асинхронные
snapshots соединены с закрытыми light/skin rows по точному frame. Первая попытка
Домино v1 сохранена отдельно: A frame2739 прошёл, B frame2833 имеет
`cameraMatched=false`; это **не успешный A/B/A** и не исправленный задним числом PASS.

Camera: 8 745 успешных API-подач, failures/stateMismatch/late/missed=0.
Direct ordinary: 3 739 532 client enqueue; selected ordinary: 1 448 691;
API/post-commit/transition faults отсутствуют. Mesh server audit: **5 788 524**
успешных renderer API returns, errors/invalid handles=0, `queue_exit_normal`.
Server SUCCESS означает принятие работы API, не завершение GPU; `early` означает
до первого оставшегося D3D draw, не автоматическую принадлежность direct lane.
Ни один из этих счётчиков не даёт credit weighted geometry или light server ACK.

Light lifecycle events: 41 Create, 21 Destroy, result errors=0. Последние
20 источников Алфеи остаются owned в frame9120; shutdown Destroy для них в журнале
нет. Полная очистка lights этим прогоном не доказана. Сохранены сообщения renderer
о начальном handshake retry, null sampler и OMM memory budget.

Переходы загрузились за 10,51 с и 8,51 с, после них кадр продвинулся на 64 и 25.
Root просмотрел `domino-skin-light-v2/native-A.png` и `alfea-return.png`:
переходный голубой Домино и светящаяся Алфея сохранены. Улучшение физического
освещения, полноценное прохождение и вся геометрия персонажей не заявляются.

## Проверки и происхождение

- Новый [анализатор](../../research/rtx-remix/analyze_native_skin_source.py):
  **233 checks / 14 CPU tests**, без ошибок. Проверены conservation, unsigned/finite,
  порядок кадров/call/submit, обязательные mismatch samples, sparse matches,
  duplicate keys, partial rows, file identity и PID gates с собственными fixtures.
- Frozen native observer v4: **56 CPU checks**; [weighted byte capture](winx-remix-native-weighted-vertex-source-2026-09-13.md)
  — отдельные **535**. Общая palette math: [33/33 exact + pc-skin-render5/5](winx-remix-skin-palette-math-2026-09-13.md).
- Native skin log: 8 744 завершённых frame rows, диапазон0…9141; лимит16MiB
  не достигнут, незавершённого sample suffix нет. Пропущенные frame rows не
  объявляются нулевой работой. Bit difference сам по себе не означает выход
  за допуск; здесь оба показателя фактически нулевые.

Installed adapter **D12684EE…C8A0A** собран из `observer-v4/source`; 41 source hash
сверен с install receipt. Client **4BB7BB4F…783E**, server **A99FEF75…E4F**,
stock renderer **F7C31082…09F**, игровой EXE **C27EA9DB…62CDB**. Полные SHA в manifest.
Поздняя LF-нормализация source не изменяла канонический текст; raw и normalized
hashes сохранены в weighted-source checkpoint, повторная поведенческая сборка
ради окончания строк не выполнялась. Новые vertex observer edits и следующая
DLL **6CCBE152…** в этот запуск не входят.

После этих проверок root обнаружил ошибку нашей dependency-list правки в трёх
других wrappers (`Test-NativeCamera`, `Test-MaterialChannels`, `Test-SurfaceResources`):
`Join-Path` получил массив вместо одного ChildPath. Их следует исправить двумя
`Copy-Item`; это ошибка обвязки, эти wrappers не исполнялись проверками535/56.
Исторические snapshots и hashes не переписываются, текущие wrappers не получают
credit от сборки установленной DLL.

Watcher восстановил bridge config в **08:45:05.255**. Шесть ini/user/saves
побитно совпали с before. Для bridge проверена цепочка old before = **immutable
before backup следующего запуска 084907**: текущий bridge.conf к моменту аудита
уже был его временным конфигом. Первый inventory assertion на текущем конфиге
сохранён как failed attempt; это не ошибка закрытого запуска. Новые live-журналы
при этой проверке не читались. Анализатор и автор checkpoint игру не запускали.

Деформация B2–B4 установленным renderer остаётся отдельным
[подтверждённым GPU blocker](winx-remix-skin-api-paired-checkpoint-2026-09-13.md).
Совпадение native palette и захват исходных bytes подготавливают следующий шаг,
но не заменяют проверку итоговой деформации.
