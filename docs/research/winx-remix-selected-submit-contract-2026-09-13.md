# Selected draw: CPU-контракт, 13 сентября 2026

Новый потребитель подаёт native-пакет для объекта, уже выбранного оригинальной игрой, в момент его фактического indexed draw. **1544 CPU проверки PASS; прежний direct suite на том же снимке — 1132 PASS.** Это собственная обвязка Remix. Оригинальные producers продолжают работать; новый потребитель не вызывает их дополнительно и не изменяет восстановленную игровую логику. Здесь зафиксирована проверка исходников и CPU-контракта; игровой запуск и GPU-проверка самого selected consumer в отчёт не входят.

[Машинный манифест](../../research/winx-remix-selected-submit-contract-2026-09-13.json) связывает исходники, исполняемые тесты, результаты и сохранённые неудачные попытки. Все **84 CPU artifact hashes**, включая 14 исторических файлов, и **59 hashes** отдельной общей GPU-регрессии перепроверены без повторного запуска тестов.

| Граница | Условие и результат |
| --- | --- |
| Native input | Текущий before-Prepare пакет поддержанного static/partition/inherited RenderNode, ordinary DXMaterial mode 2, single StdLayer, UV0 без преобразования; поддержанные unweighted list/strip. Сохраняются общие world/material/layout helpers и прежние ограничения native cohort. |
| Фактический вызов | `originallySelected`, текущие scene scope, Model call и mesh submission, owner/root/update witness; материал уже установлен оригинальным renderer. Повторный фактический вызов Model получает собственный API instance, при этом mesh/material ресурсы переиспользуются. |
| Геометрия и текстура | Exact bound VB/IB/declaration, offset/stride/indexed range и declaration elements; current transport identities/generations и покрытые CPU ranges с ранее проверенным upload. Bound texture должна совпадать с текущим native texture witness; writable/content/lifecycle изменения отвергают пакет. |
| Матрицы и состояния | Текущие native/world/material поля сверяются заново. D3D world/view/projection должны совпадать с пакетом и принятой камерой данного кадра; target — primary. Проверяются texture operations/args, sampler и общие mapped render states. |
| До commit | Изменение данных, неготовый ресурс, reentry или `bad_alloc` дают `false`: прежний путь остаётся доступен. Через внешние COM/API вызовы не переносится используемый borrowed указатель на CPU bytes/layout; расширенные вершины принадлежат обвязке. |
| После commit | Перед единственным вызовом `DrawInstance` выставляются commit и `selectedCalls`. После входа в API возвращается `true`, включая API error или последующий `bad_alloc`: повторная подача через fallback этого draw запрещена. Ошибка или потеря currentness ставит fail-stop latch selected-пути; последующие вызовы остаются на прежнем пути. Уже отправленный instance отозвать нельзя. |

Selected-путь не убирает объект из исходного selection и не заменяет обход всех Models целой группы. Это отдельная граница от [offscreen direct consumer](winx-remix-independent-submit-contract-2026-09-13.md), который прекращает только собственное добавление целого support в visibility vector. Общими остаются `PrepareNativePacket`, mesh/material cache и [ownership API ресурсов](winx-remix-resource-ownership-2026-09-13.md). Полный отказ от D3D и доказательство долговечности произвольного игрового объекта не заявляются.

Общий `d3d9_state_witness` закрывает изменение D3D-состояния внутри позднего getter или Release. Под существующим recursive guard счётчик serial меняется до и после каждого original setter, даже при FAILED result. Checked stamp снимается до первого COM чтения, проверяется после всех чтений и временных Release, непосредственно перед API и после API. При активной mutation, отсутствующей записи device, утрате фактического hook coverage или известном alias gap квалификация запрещена. Дополнительные COM references реестр не удерживает; временные ссылки getter сбалансированы.

Проверяемые device slots: **37, 39, 44, 46, 47, 49, 57, 59, 60, 61, 65, 67, 69, 75, 87, 89, 100, 102, 104**, отдельно shader setters **92/107** и существующие Release/Reset **2/16**. Созданные через CreateStateBlock/EndStateBlock блоки отслеживаются по QueryInterface/Release/Apply **0/2/5**. Фактические vtable slots всех живых записей проверяются повторно. Это карта реализованного покрытия, а не заявление отдельного runtime-теста каждого setter.

`ReleaseBlock` держит mutation через оригинальный destructor: reentrant Read/selected отвергаются до чтения уже освобождённой/null таблицы. `DetachedMutation` оставляет активный запрет на наблюдение во время Reset, но освобождает guard вокруг внешней операции. CPU-тест подтвердил доступ другого потока через bounded `try_lock`, отказ current reads в этом окне и невосстановление старого stamp после завершения. Сам системный D3D Reset этот fixture не выполняет.

Проверки нового fixture включают точные triangle/world/material/sampler payload, повторные Model calls, current/native/binding/resource отказы, поздние настоящие wrapped setters из GetRenderTarget и Release, FAILED setter с единственным original call, поздний StateBlock Apply, reentry при ReleaseBlock, ошибки и allocation failures до/после API. Проверена scoped нормализация alpha: исходный material input восстанавливается для сравнения только по соответствующему `ScopedOpaqueAlphaTest`; comparison mode сохраняет исходный compare op, отсутствие witness оставляет fallback, native material не меняется. Счётчики сохраняют `attempts = rejected + calls`, `calls = instances + apiFailures`; обычный API error сам по себе не увеличивает `postCommitFaults`.

Команда завершённой проверки:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Test-SelectedSubmit.ps1 -Name selected-v6
```

Wrapper собирает оба CPU EXE из одной frozen source directory; собственный watchdog ограничен 30 секундами, внешний — 35. Новый тест: **1544 checks / 5378 owned COM calls**. Старый direct: **1132 / 5224**, все прежние сценарии выполнены. Его тело побайтно совпадает с историческим direct-camera-v2, кроме двух строк guard вокруг main для reuse. Исторический счётчик 1136 относится к прежним production headers; счётчик включает Check внутри COM doubles и не является фиксированным числом сценариев.

| Сохранённая попытка | Что произошло |
| --- | --- |
| selected-v1 | Ошибка компиляции fixture: у двух таблиц пропущен pointer declarator. |
| selected-v2/v3 | Подача и геометрия прошли; assertion ожидал emission 0 при включённом `preserveUnlitColor`. Диагностика показала правильное значение 1; исправлено ожидание теста. |
| selected-v4 | PASS 1360/1132 до уточнения production-счётчика postCommitFaults; отдельный evidence сохранён. |
| selected-v5 | Прежние кейсы и исправленный counter прошли; новый alpha fixture не задал исходный bootstrap ALPHATESTENABLE. Исправлены только fixture-данные. |
| selected-v6 | Финальные 1544/1132 PASS, оба stderr пусты. |

Отдельный system D3D9 HAL + recording API `selected-common-v1` дал **697 PASS**, включая native geometry 171, material 137, transport 53 и texture 83. Там **selected consumer выключен**: это регрессия общего backend, не GPU-доказательство нового selected-пути. Тесты не запускали игру, bridge или оригинальные native инструкции. SUCCESS recording API не означает renderer acceptance либо GPU completion; в клиентском bridge `DrawInstance` возвращает результат enqueue без отдельного ACK. Следующая игровая проверка и server audit должны иметь отдельный закрытый checkpoint.