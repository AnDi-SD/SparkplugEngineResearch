# Native owner → подтверждённая Remix API submission

13 сентября 2026. Адаптер теперь связывает уже выполняемую native-подачу
геометрии и материала с **текущими scene/root, support, Model и world**.
Закрытый запуск Домино → Алфея подтвердил эту связь для всех **446 / 871
поддержанных native instances на записанных мировых кадрах**. Это законченный
этап аудита владельца одной операции; самостоятельный экспорт невидимых объектов
и постоянная идентичность объектов ещё не реализованы. Расширение D3D visibility
сохранено, дополнительной подачи геометрии по owner-записям нет.

Run `play-rtx-20260913-024846-177`, PID **19812**, завершён. Проверенный x86
adapter owner-v6:
`7C17983E51B83117EF68E23473981F348FF22790AF53A11922DB4F97BCF77CE4`.
Сохранён также **более ранний** x64 owner-v3 compile-only:
`1476019DFF358FD4C349EDB6F2B6A32276529502C1DE3FE1837F5F594A51DE1F`.
Он не является сборкой финального RenderNode-среза; native hooks работают
под x86. Хеши, проверяемые summaries и ссылки на raw evidence находятся в
[JSON-манифесте](../../research/winx-remix-native-owner-submit-2026-09-13.json).

Использованы [исходный owner-hook контракт](winx-remix-owner-hook-contract-2026-09-13.md)
и [дополнение для inherited RenderNode](winx-remix-rendernode-owner-contract-2026-09-13.md).
Они основаны на общей ABI и существующих CP7/10–14 evidence; при подготовке
этого checkpoint игра, native code, эмулятор и GPU повторно не запускались.
Восстановленная логика и игровой EXE не менялись. Новые wrappers, guards,
serials, учёт и fixtures — собственный платформенный код адаптера.

| Граница | Что используется и проверяется |
|---|---|
| Static / Partition | Exact support `6E6604 / 6F4528`, complete adjustments `+14 / +10`, scene и живое членство в root registry |
| Inherited RenderNode | Support `6DCADC`, complete=`support−B4`, self=`complete+124`, scene=`complete+3C`; произвольная ненулевая primary identity сохраняется как snapshot, без списка derived classes |
| RenderNode world | Указатели строго `complete+138 / +178`; конечная скопированная world совпадает с текущей native world и actual draw world |
| Model | Exact primary `6EAA58`, текущий `model+58` после original pre, actual support argument, main camera/frame/thread и новый serial операции |
| Собственный mesh call | Return address **479DF3** в существующем mesh hook; pre/post callbacks с тем же mesh не получают чужой owner credit |
| Registry | Тот же bounded обход root/children/zone; occurrences для vector20, vector30 и payload78 сохраняют node, source, ordinal и повторы, unique supports остаются отдельным списком |
| Retirement | Direct Scene `45E5D0`, PartitionNode `4264D0`, RenderNode `425050`: атомарная инвалидация перед единственным original destructor call |

Установлены четыре vtable hooks и три direct lifecycle hooks с проверкой
точных slot/entry bytes. Wrappers передают оба полных DWORD arguments и полный
EAX, учитывают boolean только по AL, восстанавливают вложенный TLS при обычном
выходе и SEH. Оригинальные prepare, очереди, Model pre/post, material/controller
updates и native mesh submit выполняются в прежнем порядке ровно один раз.
Наблюдатель не вызывает их повторно и не чинит игровые dirty flags.

Queued Model определяется по **своему support argument**, даже когда support
Draw уже вернулся. Capture выполняется на собственном mesh call после pre;
после успешного существующего API DrawInstance повторно проверяются живые
identities, vector membership/count/first ordinal, world и scene/root epoch,
затем начисляется owner-use. Отказ в owner qualification не отменяет уже
поддержанную native/D3D подачу: она остаётся без owner credit. Это учёт
происхождения, а не новая причина подавлять оригинальный draw.

RenderNode Enabled (`complete+B0 & 200`) проверяется консервативно. Native Draw
проверяет его перед enqueue, но queue flush может исполнить Model без повторной
проверки Enabled. Поэтому отказ после изменения флага — ограничение адаптера,
а не доказанное правило оригинальной очереди. Visibility stamp не принимается
за Hidden, Enabled или object generation.

| Проверка | Результат и граница |
|---|---|
| Owner CPU `owner-v5` | **142/142**: прежние 114 сохранены текстуально, добавлены 28 RenderNode; owned literal ABI data и две собственные DWORD slots, без исполнения кода игры/GPU |
| Registry CPU | **61/61**: vector20, null gaps, повторения в одном/разных nodes, unique support list, unknown secondary отказ; сохранён существующий build/run log |
| Material integration `native-owner-v1` | **541**, включая **171 native source / 137 native material**; настоящий system D3D9 и recording API. Этот тест выполнен до inherited RenderNode extension |
| Owner guards | Call once/full EAX/AL, queue вне support scope, callback callsite, live mesh после pre, membership/world/frame/root/epoch, SEH restoration, derived primary, Enabled/scene/matrix pointers, retirement до original callback |

CPU fixtures проверяют нашу обвязку на собственных данных. Они не исполняют
original-address hook install и destructor trampolines. Поздний owner-v6 live
проверяет установленный путь отдельно от этих fixtures. Первые попытки CPU
owner-v1–v3 сохранены в owner-v4 evidence: ошибка объявления fixture, конфликт
literal global mapping и исправленная wrapper-диагностика ExitCode. Вместо
замены чужого mapping введены только test-owned pointer-address inputs.
Для owner-v5 первоначальный прямой PS1 вызов остановился execution policy
до исполнения; проверка прошла через per-process `-ExecutionPolicy Bypass`
без изменения системной политики.

| Закрытый owner-v6 | Результат |
|---|---|
| Домино | **446 owner = native material = native geometry**, 3302 кадра; первый frame **342**, последний 3646 |
| Алфея | **871 owner = native material = native geometry**, 362 кадра; первый frame **31633**, последний 31997 |
| Owner totals | **1 787 994 qualified/used**; 1 932 785 captures; modelFailures=0 |
| Samples | **5777** успешных submit samples, все `outside_support_scope`; 5047 RenderNode / 730 Static; 4890 с повторным Model membership, 158 с повторной owner registration |
| Owner отказы | no_scope **69 253**, scene **298 433**, support **3302**; callsite/registry/model/mesh/world/retired — 0. Scope категории не раскрывают индивидуальную причину каждого пропуска |
| Native material / geometry | **1 843 920 uses**, material mismatches/rejections=0; **743 upload generations**, byte/layout errors=0 |
| Camera | **3668 API submissions**, failures/state mismatches/late updates=0; отдельные **27 963 missing-current-apply windows** сохранены |

Равенство owner/material/geometry проверено для каждого из 3664 записанных
мировых кадров. Это не процент всех объектов сцены: cohort геометрии/материалов
остаётся прежним, остальные renderables и passes сохраняют свой путь. Каждый
фактический вызов Model учитывается отдельно; повторные references и serial
операции не объявляются стабильными уникальными instances.

Движение в Домино доказано парой `domino-owner-ready-state.json` →
`domino-owner-moved-state.json`: оба active state **4**, позиция изменилась с
`[-1693.67749, -0.05409, 738.64203]` на
`[-893.31592, -268.70392, 738.64203]`, view изменился, projection совпал.
Основной агент просмотрел `domino-owner-moved.png`. Ранние `before-state` и
`motion-state` остались в tutorial **71** с одинаковой позицией и не используются
как доказательство движения. После движения у уступа/воды GameStateLog содержит
переходы `4 → 53 → 64 → 56` — game-over, а не установленный crash renderer.

Обе первые попытки Sweep в Алфею сохранены как `test_interrupted`: запрошенный
пункт не подтверждён. Основной агент наблюдал overshoot при `.08` секунды и
пропущенные нажатия при `.001`; третья попытка с `.01` подтвердила selection
**26**, загрузила Алфею за **10.02 с** и получила `loaded_and_presenting`.
Снимок `levels/27-Alfea01_01-attempt-3/idle.png` просмотрен основным агентом.
Оба idle/moved snapshots Алфеи имеют active **70**, поэтому не доказывают
движение по Алфее. Изменения длительности — диагностика автоматизации ввода,
не изменение renderer или игрового уровня.

На кадрах **3647–31609** имеется **27 963** native material/geometry кадра
по **2 instances**, owner=0, main camera applies/submitted=0. Они исключены
из покрытия мира. GameStateLog подтверждает фазу game-over перед Алфеей, но
каждый raw frame не сопоставлен с точным UI state. Прежний D3D camera fallback
сохраняется; эти окна не скрыты утверждением «у камеры нет ошибок».

Всего записано **3495 retirement events**: RenderNode **3474**, PartitionNode
**18**, Scene **3**. Кадровые totals содержат RenderNode=1970 и Scene/Partition=0.
После последнего Present **31997** на frame **31998** пришли ещё **1504 + 18 + 3**
события; поэтому lifecycle проверяется также по event-строкам, не только по
EndFrame counters. Эти наблюдения доказывают срабатывание известных границ,
но не полноту покрытия graph mutations или постоянную идентичность объекта.

Предыдущие запуски сохранены без переписывания. Owner-v4
`play-rtx-20260913-023352-886` закрыт на splash без owner log: Initialize был
после scene-audit hook, VerifiedImage увидел уже перехваченный `46D270` и
отказал. В v5 порядок изменён на owner → scene audit, добавлена явная причина
init-disabled. Это setup failure, не подтверждённый crash. Owner-v5
`play-rtx-20260913-023650-451` уже дал **730 owner instances Алфеи на 654 кадрах**,
но **0 Домино** из-за неподдержанного тогда RenderNode. Все 1460 sampled submits
были queued, 294 samples сохранили повторные root registrations; shutdown дал
3 Scene и 18 PartitionNode events. Новое расширение устраняет именно этот
пробел cohort, не исправляет задним числом исторические результаты.

Frozen evidence — `local-data/rtx-remix/native-owner-tests/closed-report-v1`:
36 файлов выполненного owner-v5 source snapshot, hashes всех **235 файлов run**,
inventory предыдущего material test source, тестовые EXE/logs, wrapper/analyzer
snapshots и отдельные effective/before/after runtime configs. Все 36 source
files совпали с рабочими при сборе checkpoint; для registry сохранён существующий
log, source snapshot при сборе не выдаётся за ранее записанный build-time hash.
Хеш запущенного DLL независимо совпал с launch, install и build-v6.

`user.conf`, `winx.ini`, **`.trex/bridge.conf`** и все четыре `Media/Saved` файла
совпали побайтно с before; лишних сохранений не появилось. Автоматический watcher
восстановил bridge config в `02:57:51`; повторного restore не выполнялось.
Предыдущие adapter/canonical DLL сохранены в `native-owner-tests/install-v6`.
Game EXE hash совпал с launch. Все четыре source logs ниже собственных лимитов.

Фактические `<run>/workdir/rtx-remix/logs` подтверждают успешный cleanup обеих
сторон bridge. Renderer оставил **41 common device object not disposed**,
предупреждения sampler/interface и информацию о mesh без UV. Bridge32 содержит
**1727 DirectInput warnings** (`200:634`, `208:1092`, `205:1`). В этом run нет
сообщения об OMM budget exhaustion; это не доказательство исправления старой
проблемы. Общий device/resource lifecycle не закрыт.

Следующий блок [этапа 3](winx-remix-direct-scene-plan-2026-09-12.md) — подтвердить
producer update границы ordinary Static/RenderNode и полные события
registration/removal/reset/transfer, затем выделить свежую native owner-подачу
с корректным lifetime. Пока не доказаны durable scene/object identity, reuse
адресов, произвольные изменения graph и все derived destruction paths, нельзя
заменять visibility extension постоянным replay borrowed pointers. Кэш
«последнего видимого объекта» не вводится; lighting, остальные material/UV
пути и все уровни этим checkpoint также не объявляются завершёнными.
