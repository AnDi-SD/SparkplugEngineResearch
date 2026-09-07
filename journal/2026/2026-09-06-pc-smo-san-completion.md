# PC SMO/SAN до полного проверенного знания

Начало6 сентября2026,09:10 UTC /12:10 МСК. Пользователь разрешил менять
алгоритм исследования; остановка —100% PC SMO/SAN, не прежний deadline12:00.
Budget tokens/time не задан. Goal создан отдельно; прежний цикл завершён.

Приняты [критерии](../../docs/research/pc-smo-san-completion-contract.md).
37 direct classes — отправная точка, transitive serializer/runtime helpers
обязательны. Baseline PC direct59,054054%, PS2 direct23,648649%; это heuristic
учёт, не критерий готовности tools. Legacy31 imports/1667 evidence/124 snapshots,
platform3 imports/109 assessments/24 snapshots не переписываются.

HEAD7c2b615, большой прежний dirty worktree сохранён. No apps/assets/game/GPU/
commit/push/release; bounded guest/static/portable research. Subagents не
запускаются без отдельного явного разрешения. PS2 secondary, не новый основной
фронт. Исходные имена подтверждать, аналитические не выдавать за оригинальные.

Первый вопрос: PC generic SAN resource-loader и его FFPS/FAT transaction,
отличие от scene-only422550, registry/materialization/relationship/lifetime.
Existing Animation reader43ECC0 и owned name registry — готовые опоры.

## Checkpoint 1: общий SAN loader и PC FAT

Доказательства: [PC loader](../../docs/research/native-pc-smo-san-loader.md).
Registry78 + file-index60 + RTTI10 + resource-index61 + whole-loader50 =
259 направленных native проверок; отдельно34 static anchors и227 сравнений
результата whole native load с portable field reader/owned bindings/PRS.
bbush2554 bytes: первый422B50=79064 инструкции, второй11949; оба завершены,
все отслеживаемые allocations освобождены. PC FAT58 и layouts/maps/cursor,
duplicate-ID утечка и borrowed entry20 подтверждены независимо от PS2.

Отрицательные результаты записаны в карточке: initializer6D1C10 capped889BB8;
whole bflower/barrel/bw capped472923/41FB28/13D7892. Не возобновлялись,
лимиты не увеличены, профили whole-load этих трёх файлов выключены.
Ошибочная fixture cleanup calling convention исправлена на cdecl; это был
дефект теста, не дефект native file-index. Большие RTTI startup allocations
остались за allocator guard; их точная роль ещё не доказана.

Исправлена старая запись initializer-а Animation:6D1C10 начало,6D1C30 callsite.
Добавлены PC ABI evidence layouts, тестовый portable --inspect-owned и новый
bounded profile. Полный portable FAT loader и writer не выдаются за готовые.
Это промежуточное сохранение, не остановка цели100%.

После checkpoint1 platform ledger: 4 imports / 111 latest assessments /
32 snapshots; legacy 31 / 1667 / 124 сохранён. CTest 22/22, loader profile6/6,
workbench tests9/9 и platform tests12/12. PC direct59,054054% не изменился:
две первые отдельные PC-оценки manager-классов меняют учёт покрытия, а не
превращают ранее unrated историю в доказанный исследовательский прирост с нуля.

## Checkpoint 2: SAN writer, nested blocks, точность cubic preparation

[Подробная карточка](../../docs/research/native-pc-san-writer.md).
Original field writer43DFE0 и helper43DDC0: 64 success checks на empty и
четырёх реальных SAN; 17 проверок трёх write failures. Для bbush дополнительно
227 runtime values после native write/read. bflower/bw/bbush совпадают с
исходными fields; barrel отличается ровно добавленным field64 (6 bytes).
Это не повторение capped whole loaders: field reader/writer — независимые
завершённые вызовы с прежними ограничениями.

PC nested block writer: 163 directed checks, включая original bugs mixed
widths/empty payload/ID31 и partial failure state. Reconstructed source
совпал на 18 outputs / 1658 bytes. Whole writer очищает свой header stack;
standalone fixture manual cleanup не выдаётся за standalone native destructor.

Добавлен portable SAN field writer на существующем spStream/block core.
Полный FFPS/FAT save и сохранение skipped unknowns ещё НЕ реализованы.
Побайтное сравнение выявило и помогло исправить UInt16 tag headers и точные
x87 float-spill точки cubic preparation. Для нового math helper прошли
96 cases / 4160 bit-exact values; старый tolerance-only PRS test не обнаруживал
исчезнувший -2^-24 в bbush. Все четыре полных field output дают 64 233 bytes
для сравнения; native canonical field64 не объявляется lossless исходником.

Незавершённый failed-write30: ErrorManager4169A0 достиг unmapped formatting
IAT6D9328. Это внешняя граница теста, не доказанный native crash. Повтор
отключён; статическая ветвь43E506 показывает cleanup/false, но нет полного
runtime evidence. Неизвестные constructor/FAT/resolver/render branches
сохранены. Промежуточный checkpoint не закрывает активную цель100%.

Manifest `native-platform-pc-san-writer-2026-09-06.json` импортирован без
перезаписи истории: platform5 imports / 111 assessments / 40 snapshots.
`spAnimationSerializer` PC heuristic90->92; он является транзитивной
зависимостью и НЕ входит в старый direct37, поэтому PC direct59,054054%
не изменился. Это показывает, почему эта метрика не заменяет completion
criteria. PC all credited6,34379263%, engine14,13373860%; game UNRATED.
PS2 без изменений. CTest23/23, native writer profile14/14, output profile
6/6 (включая cubic bits), Python workbench9/9 и platform12/12; diff check чистый.

## Checkpoint 3: общий save-reference protocol

[Карточка](../../docs/research/native-pc-save-reference.md): 86 направленных
native checks и9 fingerprints. Actual PC index dispatch, FAT save-entry/name
ownership, duplicate handling, object header, first/repeated/null references.
Первый empty Animation reference63 bytes, повтор8, null4; compiled source
совпал с original на всех75 bytes. Индексация раньше relationship callback
и payloadWritten раньше header/payload не выдуманы: это исходный порядок.

Native игнорирует четыре проверенных отказа финальных seek/write; failed SAN
restore дополнительно портит terminator/pool-header и итоговый размер. Host
проверяет эти операции и сообщает ошибку, но не обещает rollback: после
неудачи контекст записи и output нужно отбросить. Полная ветвь ErrorManager
с форматированием по-прежнему не выдана за исполненную.

Portable65 checks; native profile11/11; output profile1/1; CTest24/24
(16,29s), Python workbench9/9 и platform12/12. SAN pool patch order уточнён
по фактическому I/O: value counters, time counter, restore end. Whole FFPS
save и recursive reference reader ещё открыты. Цель продолжается.

Первая отдельная PC-оценка spSerializer70[55..85] включает прежний identity/
header срез и новые86 checks; это НЕ70 пунктов новой работы с нуля.
Manifest импортирован: platform6 imports /112 assessments /48 snapshots,
legacy31/1667/124 сохранён, native-only FK clean,287 evidence links.
PC all credited6,43929059%, engine14,34650456%, direct37 по-прежнему
59,05405405%; game UNRATED. PS2 all1,66666667%, engine4,12727273%,
direct23,64864865%, game UNRATED — без новых PS2 исследований.
Строка audit «This cycle ...0.06» относится к прежнему интервалу07:24..09:00,
а не к текущему открытому goal; её нельзя выдавать за прирост этого этапа.

## Checkpoint 4: PC read-reference и nonempty resource cache

[Карточка](../../docs/research/native-pc-read-reference.md). 115 directed
native checks /4 fingerprints; native profile15/15. Expected class argument
не используется; ID и payload streams раздельные; object20 публикуется до
reader. Fresh inline size1/999 не ограничивает фактические55 bytes. fileID42
не меняет inline resolver (это не outer manager fileID-ветвь). Cached и уже
созданные pointers возвращаются без retain; failed skip тоже возвращает pointer.

Cache named MeshData: original factory/register/find/remove и borrowed
reuse исполнены. Первая fixture teardown ошибка была вызвана известными
неинициализированными owned pointers50/54; explicit empty fixture исправлена,
не выдаётся за native Initialize. Missing ID и failed payload остановлены
до sprintf/UI. После failed payload object остаётся опубликованным; native
rollback не приписан. Лимиты не повышались, прежние capped calls не повторялись.

Общий portable reader использует существующие FAT/RTTI/serializer/stream
и SAN field core, не заменяет уже опубликованный factory address. Context
владеет новыми объектами, не cache pointers; ошибки poison, retry не читает
дальше. Lifetime этого host context не выдаётся за arbitrary cyclic SMO proof.

Реальный bbush wrapped reference:58848 original instructions, actual names
registry/cleanup; portable --inspect-reference совпал на227 runtime values.
Parser-derived pools исключены. Portable167 checks; CTest25/25; writer-output
regression6/6 (64233 SAN bytes/1658 block bytes/4160 cubic bit values) сохранён;
workbench9/9, platform12/12 и diff check прошли. PS2 не исследовалась заново.
PC heuristic spSerializer70->82, ResourceManager75->77;100% не объявляется.
Продолжение: общий full-file loader, native materialization и PC mesh hook.
Checkpoint4 manifest импортирован: platform7 imports /112 assessments /
56 snapshots; legacy31/1667/124 не изменён, FK audit clean,288 evidence links.
PC all credited6,45839018%, engine14,38905775%, direct37=59,05405405%;
game UNRATED. PS2 показатели checkpoint3 полностью неизменны.

## Checkpoint 5: full portable SAN loader и DX gate

[Карточка](../../docs/research/native-pc-full-loader.md). Original outer422940
42 directed checks; exact bit2/MeshData DX-hook48;4 hashes. Profile15/15,
whole-output2/2. Source full loader160/reference266; CTest26/26. Whole original
bbush и portable complete FFPS path совпали227 values; portable4 SAN/62 tracks
совпали с independent owned field core. Последнее не original whole corpus
evidence: capped bflower/barrel/bw не повторялись. Two-live-file teardown
сохраняет имена/ссылки второго объекта. Игра/GPU не запускались.

Native root selection не равен first-nonnull: prebound пропускается, external
entry может поглотить root flag без результата. Outer Register идёт перед
SetName, inline resolver наоборот. Partial payload не откатывается native.
Portable explicit guards/RAII/poison отделены от этих контрактов.
DX hook исправлен: только bit2, только exact serialized MeshData. Ранее
bbush platform1 исполнял4AAB80 gate, но не4AA870body. Непустая mesh batch
пока unsupported, не замаскирована успешной частичной загрузкой.

Python workbench9/9, platform12/12 и diff check прошли. Manifest импортирован:
8 immutable platform imports/113 assessments/64 snapshots,289 evidence links;
legacy31 imports/124 snapshots сохранены, native-only FK clean.
PC all credited6,56753070%, engine14,63221884%, direct37=59,05405405%;
game UNRATED. PS2 all1,66666667%, engine4,12727273%, direct23,64864865%,
game UNRATED; нового PS2 evidence нет. Первая отдельная оценка DX hook65
включает прежний ABI/parser/plan, не означает65 пунктов свежего исследования.
Audit cycle delta0.06 относится к старому07:24..09:00, не текущей цели.
Продолжение: actual nonempty DX metadata/combiner/mesh materialization,
остальные concrete SMO adapters и всё ещё открытый whole native save.

## Checkpoint 6: PC DX payload и настоящий declaration object

[Карточка](../../docs/research/native-pc-dx-materialization.md).
468 directed checks/50 fresh bounded children:metadata92/19,first scan35/7,
DX wrappers/combiner97/6,payload160/6,declaration59/10,COM-declaration25/2.
Ещё11 точных static anchors. Normal/packed helper429A40 теперь завершается
полностью, с actual CPU buffers/mesh copy/commit/declaration map/create и
temporary cleanup. COM — явная fixture, не GPU; whole SMO loader ещё не пройден.

Исправлены существенные прежние описания:17byte header затем обычный INDEX
codec и VERTEX codec, не rawVB/rawIB. Сам429A40 игнорирует header values.
Mesh84 — borrowed spPCVertexDeclaration pointer, не numeric format code;
PC ABI/offsetof test исправлены. Actual factory подтвердил DXMesh88h,
PCVertexDeclaration1Ch, DX wrappers20h/1Ch. Неназванный field10 declaration
оказался уже известным inherited sharedNameEntry, повторный новый unknown
не создавался. Base DX declaration registration не имеет factory.

Native hazards: metadata ReadHeader-null может статьtrue; missing field
повторяет предыдущее output в batching; packed copied84/stored120 bytes;
32bit index copy при combiner INDEX16; ignored Create/Lock/Unlock HRESULT;
declaration allocation tail24CC послеterminator; failed COM create всё равно
кэширует wrapper с null COM pointer. Safe source guards не отменены.

Две scouting assertion ошибки были в fixtures: metadata failed-header неверно
ожидалfalse; packed cleanup ошибочно считалEBX живым vertex pointer, хотя
native заменяет его наcount. Они исправлены по фактическому control flow,
не обходом ограничений. Declaration sentinel не обязан быть концом allocation.
Новых capped native calls нет, старые не повторялись. Broad readonly DB query
была остановлена5/10sec progress guard; точечный lookup дал logo_screen mesh.

Manifest13:06:19UTC импортирован:9 immutable platform imports/116 assessments/
72 snapshots/292 evidence links; legacy31imports/124snapshots сохранены,
native-only FK clean. PC all credited6,84038199%,engine15,24012158%,
direct37=59,05405405%,gameUNRATED. PS2 all1,66666667%,engine4,12727273%,
direct23,64864865%,gameUNRATED — без новогоPS2evidence.
Первые отдельные PC оценки DXMesh70/PCDeclaration65/DXDeclaration55 включают
прежний identity/static/source срез, не означают190 пунктов свежей работы.
Workbench9/9,platform12/12,diff check и пересборка прошли.
CTest26/26 также прошёл после ABI/test rename (18,75sec).
Цель продолжается: full hook/FFPS на реальном меше и source reconstruction
declaration emitter/mesh reader через единое ядро, затем native save closure.

## Checkpoint 7: whole DX batch и исходники declaration

[DX](../../docs/research/native-pc-dx-materialization.md),
[declaration](../../docs/research/native-class-sp-vertex-declaration.md).
Original whole4AA870:91checks/3children — triangle, два live меша и unchanged
193byte logo_screen payload в явно составленном envelope. Actual FAT/RTTI,
serializer factoryexact14/header/secondary reader/CPU buffers/mesh copy/cache
и name dispatch работают. Max53284 instructions/65152 из64KiB heap; лимиты
не изменены. Whole FFPS/scene/GPU не объявлены. COM и name-storage — fixtures.
Первый scout забыл required RTTI tree и вошёл в известный cold414C60: guard
остановил allocation. Он не возобновлялся; отдельный корректный startup input
использует прежний доказанный tree. Unknown constructor не подменялся.

Два меша имеют общие wrappers/declaration, disjoint ranges и local index bytes.
FAT publication происходит послеpayload; завершение сбрасывает active global,
но оставляет allocated combiner живым. Fixture cleanup отделён от native.
Planning component940 и converted FVF152 не обязаны совпадать; ошибочный
host guard убран, проверки вместимости сохранены.

Emitter4C9A00→13D6F00 перенесён в original-name классы:
106 masks/8656 bytes совпали точно, включая capacity и untouchedCCtail;
все32 одиночных bits +64 mixed +направленные cases. Native lifecycle59checks:
реальный clone-map/name copy, blank FVF/COM/opaqueC; reinitialize теряет oldCOM,
cache key не меняется, failed Create/Bind могут вернутьtrue. Ни утечка, ни
ignored HRESULT не выданы за безопасный контракт приложения.
Source declaration/renderer cache/explicit mesh binding используют existing
ядро; shared_ptr — явное safe host отличие от borrowed native84. Без renderer
путь остаётся CPU-buffer-only. COM adapter и whole FFPS source ещё предстоят.

Source338 checks,27/27 CTests прошли25,79sec. Native profiles3/3,1/1,4/4.
Immutable manifest13:43:31UTC импортирован:10imports/116latest assessments/
80snapshots/292links, legacy31imports/124snapshots и native FK сохранены.
PC all credited6,91814461%,engine15,41337386%,direct37=59,05405405%;gameUNRATED.
PS2 all1,66666667%,engine4,12727273%,direct23,64864865%,gameUNRATED; новыхPS2данныхнет.
Direct37 неизменен: новые стыки входят в обязательные зависимости, а не в
старый narrow denominator. Эти проценты не готовность четырёх приложений.

Очередь получила отдельный PC mesh materialization front, без искусственной
предпосылки в виде capped visibility assignment. Submission зависит от него;
spatial frontier сохраняется. Обнаружено, что простая priority сортировка
ставила consumer перед loader dependency; очередь исправлена на стабильный
dependency-aware порядок, без требования100% prerequisite до независимого теста.
Цель продолжается; следующие source concrete mesh adapters/whole FFPS/save.

## Checkpoint 8: concrete mesh readers и full FFPS composition

[Карточка](../../docs/research/native-pc-dx-materialization.md).
Native6 readers156 checks плюс actual422B50 с непустым4AA87044 checks;
explicit292byte FFPS содержит empty Animation root и один mesh. Root остаётся
правильным, FAT очищен, mesh/declaration живы. Final cursor138, неEOF292,
потому что outer пропускает prebound mesh.78325instructions/65040heap,
неизменённые100k/2sec/64KiB/30sec guards; новых capped calls нет.
COM/stream/name/RTTI containers — явные fixtures, не real scene/GPU proof.

Source/native сравнение60 полей включает **одинаковые input bytes**, cursor,
FVF/stride/sizes/count и все geometry bytes. Source131 checks:6 readers,
полный composed FFPS с двумя мешами, две живые загрузки, освобождение первой,
7 unsafe payload guards. Codecs Index/Vertex переиспользованы с проверкой
remaining payload **до allocation**. Source hook использует common registry/
header/payload и post-payload FAT publication, explicit renderer/declaration,
scoped combiner с безопасным host lifetime. Native live-combiner anomaly
не замаскирована. Standalone packed84/stored84 отличается от combiner84/120.
Field80 label UVCount был неверен; алгоритм bit-count уже верный, rename-only.

Два source RTTI дефекта были видны только при честной whole-file composition:
cross-TU static registration могла потеряться доbase initialization;
создание AnimationSerializer не активировало lazy target RTTI. Добавлены
deferred static lifetime queue и явное удержание wire targets в serializers.
147 startup call sites адаптированы; строгая Register и FAT gate сохранены,
9 новых SparkBase checks. Это не доказательство native protectedRTTI startup.
Первый rebuild обнаружил только mismatch std::byte/uint8 capture helper;
исправлен generic hex formatter. Последняя сборка и28/28 CTests прошли18,69sec.
Workbench10/10,platform12/12,new bounded profiles6/6 и1/1 также прошли.

Immutable manifest14:43:08UTC импортирован. Первые PC assessments
MeshDataSerializer60/DXMeshDataSerializer65 включают прежний identity/layout
срез, не означают125 пунктов новой работы. Hook82→85,DXMesh75→77.
PC all credited7,09549795%,engine15,80851064%,direct37=59,05405405%;gameUNRATED.
PS2 all1,66666667%,engine4,12727273%,direct23,64864865%;gameUNRATED, нового
PS2 evidence нет. Direct37 — старый narrow denominator, не полнота области цели
и не готовность четырёх инструментов.

Следующий связанный front уже начат: Node reader/writer/children, затем
RenderNode/Model/material/skin. Предварительные Node результаты ещё не
включены в эту оценку. Полный native save, реальные scene files и backend
остаются обязательными. Цель продолжается,100% не объявлены.

## Checkpoint 9: Node sections, child ownership, real tiny SMO

[Карточка](../../docs/research/native-pc-node-serialization.md).
162 native checks включают24 assertions в намеренно остановленных
pre-callee fixtures. Десять exact source/native reader/writer/graph сравнений
и два полных Node-record настоящего `object.smo` совпали. Native corpus test
исполняет staged outer422940, **не whole422B50**. Новые source226 checks,
29/29 CTests20,15sec; bounded profiles11/11 и8/8, workbench10/10, platform12/12.
Тест цикла очереди зависел от положения первых двух items; исправлен fixture,
который теперь явно создаёт двустороннюю dependency. Алгоритм не ослаблен.

Exact PC NodeSerializer factory14, scalar/child/read/index/write через common
ядро, repeated same-parent child no-op, shared source owners и explicit
prebound owners. False Static/Animated не очищают прежнееtrue; writerfalse
не гарантирует native roundtripfalse. AbsentAnimated сохраняетdefaulttrue.
NULL camera =identity; explicit degenerate camera остаётся host-safe rejection.
Collision пока явно отклоняется; originalNULL edge остановлендоdereference.

Два original whole-call probes достигли100k: composedNode422B50@88FDE3 и
gameover outer@89A355 (third factory, reader2,attach0,cursor254). Оба отключены,
не повторялись/не возобновлялись; source gameover success не засчитан как
native differential. Статическая локализация/оставшиеся consumers обязательны.

Immutable Node manifest импортирован:12imports/119latest platform assessments,
292links; legacy31imports/124snapshots/FK сохранены. Первая PC оценка
NodeSerializer75[58..87] включает прежнийidentity/layout, а не75 новыхпунктов.
PC credited all7,19781719%,engine16,03647416%,direct37=59,05405405%;gameUNRATED.
PS2 без изменений:all1,66666667%,engine4,12727273%,direct23,64864865%,gameUNRATED.
Старый direct37 не знаменатель цели и не готовность четырёх инструментов.
Продолжается связанная цепочка RenderNode/Renderable/Model/material/fog/mesh.

## Checkpoint 10: RenderNode → Model, inherited sections and exact writers

[Карточка](../../docs/research/native-pc-scene-serialization.md).
110 native checks включают4 assertions stop-before-NULL-diagnostics.
15 source/native rows совпадают по input, result/cursor/scalars/edges и всем
output bytes для13 успешных writes; два scalar failures совпадают какfalse.
16/16 bounded children,230 new source checks,30/30 CTests20,10sec,
workbench10/10, diff check. Native maxima32384instructions/57408heap дляreader,
writer15937instructions; лимиты прежние, новых capped calls нет.

Все три serializer factories прямо подтвердили14. Writer entries взяты из
настоящих secondary vtables: Renderable47F7A0→1402630, Model4935F0→1404A30,
а не из ошибочно воспринимаемых unprotected body labels47F7B0/493600.
RenderNode aliases сохраняются/retained за каждое вхождение. Model проходит
common inline/prebound resolver, inherited sections и native recursive index.
Отсутствие mesh допустимо; explicitNULL идёт вдиагностику. Alpha UInt32,
priority UInt32, projection1 всегда с Begin/End UInt32 length framing.

Первый writer scout забыл отдельный save index и faulted4673AC; исправлен
настоящим clear/index workflow, не seam. Whole NULL diagnostic упёрлась
в известную внешнюю formatting boundary4169DF→0033D03C и отключена.
Отдельный stop469288 доdiagnostic проверяетNULL/emptyvector и cleanup,
не притворяется полнымreturnfalse. Все прежние caps отключены безповторов.

Source uses existing core/context canonical owners; analytical SectionCursor
проверяет envelope, не вводит второйcodec/выдуманныйnativeclass. Skin пока
явно отклоняется вместо молчаливой записи однойbase секции. Nonempty mesh/
material/fog, lifetime/errors/lossless/wholeSave/backend остаются обязательными.

Immutable scene manifest:13imports/122latest platform assessments/292links;
legacy31imports/124snapshots/FK безизменений. Первые отдельные PC оценки
RenderNodeSerializer75, RenderableSerializer70, ModelSerializer70 включают
старыеidentity/layout, не215 пунктов чисто новойработы.
PC creditedall7,49113233%,engine16,68996960%,direct37=59,05405405%;gameUNRATED.
PS2 all1,66666667%,engine4,12727273%,direct23,64864865%;gameUNRATED, нового
PS2 evidence нет. Direct37 не знаменатель цели, не полнота четырёх приложений.
Продолжаю nonempty resource links, а не останавливаюсь на этом checkpoint.

## Checkpoint 11: nonempty Model/Fog, exact codec and native lifetime

[Карточка](../../docs/research/native-pc-fog-serialization.md).
99 native checks:60 scalar/error/realSMO,33 owning graph,6 blankclone/native map.
9 exact source/native input/state/output rows,151 новых source checks (Scene
suite381),30/30 CTests20,80sec.12-case profile+отдельный clone passed; профиль
теперь включает13 cases. Лимиты прежние, новых caps/API failures нет.

Fog factory28/Serializer14 прямо исполнены; field0 ровно20rawbytes/23сheader.
Проверены raw enum/negativezero/qNaN/Inf безrenderer. Настоящий logo_screen.smo
Fog FATID5/header416/section424..447 совпадает вnative read/write. Native
поздняя ошибка оставляет ранее прочитанные слова изменёнными; strictsource
отвергает неполный field доmutation. Nativewritefirstfail не создаётoutput.

Actual Model→Renderable→Fog inline/prebound/repeat/clear и recursive index/
write замкнуты. Clear sole-owned Fog удаляетобъект, но FAT содержитfreedaddress;
повторное обращение не запускалось. Source context удерживаетcanonical owner,
это явное безопасное lifetimeотличие. ActualFogclone41A8E0 blank/default;
первый fixturecleanup забыл separateCloneManager24/global74E060. Послеобычного
destructor всёосвобождено, nativeleak не заявлен.

Immutable Fog manifest:14imports/123latest platformassessments/292links;
legacy31imports/124snapshots/FK сохранены. Fog90безповышения, перваяPCоценка
FogSerializer80 включаетпрежнююidentity/wire; Renderable70→75,Model70→72.
PC creditedall7,60982265%,engine16,95440729%,direct37=59,05405405%;gameUNRATED.
PS2 безизменений:all1,66666667%,engine4,12727273%,direct23,64864865%;gameUNRATED.
Direct37 не полнотаобластицели/готовностьинструментов. ДалееMaterial/mesh,
Texture/Skin иremainingSave/scene/backend; цельпродолжается,100%необъявлены.

## Checkpoint 12: PC material ABI/scalars, Fog physical name

[Карточка](../../docs/research/native-pc-material-scalar.md).
Actual PC MaterialData41A390 allocationBC, common prefix78; прошлые80/C4
ошибочно переносили PS2 alignment. Interface6DE9D0/this+14, actual colors78/
88/98/A8/B8; PS2 таблицы не изменялись. Physical Material/Fog name10 не меняет
engine BaseObject RTTI. Fogclone удерживает имя, MaterialData clone оставляет
NULL/defaults. Common4671F0 игнорирует FATnameFog поRTTI; первая обратная
гипотеза провалилась вnative Иsource и исправлена, bypassне вводился.

Material:26factory+27interface/clone/pass+20scalar native assertions.
Fog:5physical-name+9named-graph assertions. Five material rows and one Fog
row exact input/state/output. Native pass remove shifts tail and decrements
count once; source uses canonical shared ownership and index guard. Rawalpha2
не нормализуется, colors use nativefloat reciprocal/x87-truncate contract,
NULLcontroller ignore and UInt32 field6 framing preserved through common core.
New Material suite136checks, Scene401,31/31CTests32,25sec.

Whole logo MaterialData native scout exceeded32KiB request guard, no exact
failing request/PC logged. Incomplete layer/RTTI startup is a lead, not proven
cause. Mode disabled withoutretry/resume/raisedcaps. No whole real material/
render/lossless/error completeness claim. No game/GPU/assets/PS2 research.
Source deliberately rejects known-but-unrestored layers/controllers. Continue
connected material→layer→texture/controller work; goal remains active.

Финальная проверка после MaterialTexture ABI correction:31/31CTest8,89sec;
material profile14/14. Sevenfactory26+runtime27+scalar20+Fog14=87assertions.
Nested MaterialTexture actual68, старое6C assertion было ошибочным; derived
target word68 отделён отbase, его роль покаopaque. Immutable manifest12:
15imports/127latest independentassessments/292links; legacy31/124/FKclean.
PC all7,99181446%,engine17,80547112%,direct37=59,05405405%;gameUNRATED.
PS2 unchangedall1,66666667%,engine4,12727273%,direct23,64864865%;gameUNRATED.
Первые4 PC class assessments включают прежниеidentity/staticfindings;
MaterialData90/Fog90неповышались. Direct37не знаменатель цели/готовностьapps.

## Checkpoint 13: standard material graph and real PC runtime class

[Карточка](../../docs/research/native-pc-material-standard-graph.md).
Header42F4C0(MaterialData/DXData) после8bytes игнорирует оба слова и создаёт
**spDXMaterial797B39EC**, не wire6160348B. Actual startup6D4C80 регистрирует
его с отдельным spDXMaterialSerializer177E2F26/factory4B0DD0. Начальный
fixture содержал только MaterialData/StdRTTI; reader прошёл, index получил
нулевой ID незаданного runtime record. Исправлены входные RTTI/registry по
оригиналу, не добавлялись class0/factory/vtable bypasses.

DX factoryBC/secondarythis+14; ambient/emissive(0,0,0,0), power+B8 ctor не
пишет. Host хранит отдельный initialized flag и не выдаёт CCpoison заdefault.
Actual scalarclone копирует17words tail и rawstates/flags, в отличие от
MaterialData blankclone. Nonempty DXclone graph покаотклоняется явно.

Material→Pass intrusive(ref1), Pass→Layer→MaterialTexture **direct-delete**
(ref0,0); два nestedowner вsource теперьunique_ptr. Actual45F5E0 NULLслот
не сдвигает/уменьшаетcount, а count=max(count,index+1), дажеNULLidx7→8.
StdLayer14 создаётnested68; states8/17 девятьслов, default
[0,3,1,0,0,FF000000,2,0,0]. UV9=flag32+matrix36; ненулевойнормализуется1,
нулевойconsumesmatrix, но не сбрасывает предыдущеесостояние. Wrong36byte
UV native возвращалtrue сdiagnostic; строгийhostотклоняет доmutation.

116nativeassertions=56layer+7owner+7DXfactories+15DXheader/copy+31graphs.
11 exactinput/state/outputrows, standardgraphprofile16/16, старыйscalar
profile14/14. SourceMaterial367checks; полнаяCTests31/31,21,64sec.
Workbench10/10/platform12/12units,diffcheckpass. Неправильный unittest
import path исправлен запуском штатных testentrypoints, неправкоймодулей.
Nativeclonefixtureauxpointer sentinel очищен передnative destruction;
исходныйfaultнеобъявленnativebug. Новыхcapsнет, старыеотключённыене запускались.

Immutablemanifest13:16imports/133latestPC-or-PS2assessments/293links;
legacy31/124,FКclean. SixfirstPCgrades включают прежнееstatic/identity,
а не370newcompletionpoints. PC all8,51705321%,engine18,97568389%,95assessed;
old direct37=59,05405405%безизменений;gameUNRATED. PS2 unchanged:
all1,66666667%,engine4,12727273%,direct23,64864865%;gameUNRATED.
Старый auditcycle delta0,06 относитсяк07:24–09:00, не текущейцели.

ДалееnonemptyMaterial→Texture/Anim/UV/Color links иPCtexturematerialization;
DXShaderLayer71643E66 тожеобнаружен какреальнаязависимость иостаётсявочереди.
НетPS2credit/game/GPU/assets/publication. Цель100% продолжается.

## Checkpoint14: texture CPU codec и DX границы

[Карточка](../../docs/research/native-pc-texture-codec-boundaries.md).
Actual42DD10 игнорирует8bytes и создаёт DXTexture4AB520(4C), не CPUData.
Data/DXData header общий; runtimeDXTextureSerializer4B24C0 usesgeneric467550.
DXctor4AAEB0 acquiresrenderer+C9E8 device/AddRef, dtorотпускает device/texture.
CPUData41A2D0(4A0) read42F180/write42EE10 выполняют source-wrapper section
**с отдельным terminator**, затемlocal иnestedraw field5. Source использует
общие SectionCursor/DataBlock; поля6/0/5 writerUInt32BeginEnd,3 exactrows.
CPU source113checks; incompleteDXheaderfactory явныйunavailable, неподменаCPU.

Actual423250→4ABB70→4AB650 остановлен ровно передinternal60FDB4, безего
успешнойподмены.8 tinycases:source1×1,normalizeddestination2×2;5formats
и3compressionflags, actualCreateTexturepolicy/sourcepitch/rect/форматы.
Полногоconversion/GPUнет. Size4AAC50 executesprotectedprefix404BC6,
который**обнуляет48**; прежняягипотезанакопленияошибочна.4sizerepeatcases
rawPitch*Height иDXTfloorblocks, balancedlocks/surface/deviceowners.
Первоначальныеmissingrenderer/managerteardown/sourcewrapperterminator были
fixtureошибками; capsнеувеличены/старыене запускались.

Profile22/22;116nativeassertions=20factories+12CPU+56upload-boundary+28size,
3exactrows,source113,Ctests32/32(20,89sec),Workbench10/platform12,diffcheckpass.
Immutablemanifest14:17imports/139latestindependentassessments/294links,
legacy31/124,FКclean.6firstPCgrades включаютпрежниеidentity/staticfindings,
не385newresearchpoints;TextureData85retained.
PC all9,04229195%,engine20,14589666%,101assessed;gameUNRATED;
olddirect37=59,05405405%безизменения,не знаменательцели.
PS2 all1,66666667%,engine4,12727273%,direct23,64864865%,gameUNRATEDunchanged.
Oldauditcycle0,06 относитсяпрежнему07:24–09:00,не текущейцели.

ДалееactualnativeDXmips/palette/container→DXTexture,materialtexturelinks,
conversionattribution/implementation,sourcebackend. Noapps/assets/publication.
Checkpointне остановка:цель100% PCSMO/SANпродолжается.

## Checkpoint15: runtime flat texture, native mip copy и registry

[Карточка](../../docs/research/native-pc-texture-runtime-mips.md).
Actualstartup6D1850/1880,18B0/18E0,1910/1940 регистрируетDX/PS2/common
serializersпод**однимwire78EA082B**, masks6/8/1,operations1/2.
Virtualidentifier0B1C67BBнеединственныйregistrykey/недоказанныйruntimeкласс.
Lookup0B1CпослешестиstartupNULL.21assertions;PCбайтыкодаPS2creditнедают.

Runtime4B2950/4B27C0 — flat width,height,runtimeFormat,bytePalette,count,
packedmips. NativeData42C640/42C3B0/4ABBA0 — отдельныйsectioncodec,
temporaryTextureDataсvector16records иoriginalrowcopyвCOMstorage.
Source/runtimemipстрокиисключаютphysicalpitchpadding. Полныеprovidedchains
позволилиcompleteoriginalnativecopyбезподмен60FDB4/61039A/mipgeneration.
NativeDataнепишет44/48;runtimeattachment4ABAC0непишет18/1C/flags20.

SourceдобавленspDXTexture/spDXTextureSerializer(originalIDs,bases,inferredpaths),
Dataheader42DD10теперьсоздаётправильныйDXTextureCPUshadow,неCPUData/unavailable.
НетliveCOMclaim;physicalpitch—явнаяCPUtestpolicy. RuntimeformatInitialized
защищаетотвыдуманногоdefault44. NativeDatasourceadapter/paletteпокаopen.
СборкаисправилатолькотестовыйtypoIsTypeOf→существующийIsKindOf.

256nativeassertions=141runtime+94nativeData+21registry;14exactrows,
profile24/24,Texture451sourcechecks,CTests32/32(23,96sec),Workbench10/platform12,
diffcheckpass. Guards100k/2sec,30secchild,64KiBarena/32KiBrequestбезизменений.
No cap/retry/GPU/game/assets/publication.

Realicebat.smo35571bytes/SHA6AEC9CA21EB50FD93E89955551C3557FF19BD21260038FF6E7CF94EEF178BF72
textureoffset1336,size4160,wire78EA;outerfield3вкладываетsource/localDXnativebody.
Этоstaticreadlead,неwholeload.42EA50field3вызываетselectedvirtualreaderрекурсивно.
НайдёнspPalette591C0B9F(Base415352A1),record763DE0,factory4B2C80;completeowner
иrendererregistrationещёopen. D3DXstrings/version5.04.00.2904естьвEXE,
ноточнаяатрибуциядвухconversionbodiesпокаlead.

Immutablemanifest15:18imports/139latestindependentassessments/295links,
legacy31/124,FКclean. PC all9,12414734%,engine20,32826748%,101assessed;
gameUNRATED,direct37=59,05405405%unchanged/не знаменательцели.
PS2all1,66666667%,engine4,12727273%,direct23,64864865%,gameUNRATEDunchanged.
Oldauditcycle0,06несчитатьактуальнымgoalDelta.
Следующийузел:nativeDatasourcewrapper/realasset,materaltexturelinks,palette,
missingmips/conversion/errors/wholeSave/renderintegration. Цельпродолжается.

## Checkpoint 16 — общий texture source wrapper и native-data reader

19:03 UTC. Восстановлены shared CPU/DX source wrapper и DX native-data reader.
Оригинальный field 3 рекурсивно вызывает тот же serializer на том же объекте,
без нового header. Одиннадцать exact comparisons проверяют wire bytes, фактически
скопированные original pixels и base state. Полные tiny mip chains, включая два
embedded случая; original native-data путь оставляет DX 44/48 неизменными.
Source хранит analytical surface format отдельно и не выдумывает writer defaults.

Проверки: profile pc-texture-native-source 12/12, 117 native mip assertions +
5 palette factory assertions; regression runtime/mips 24/24; source 719/719;
fresh build и CTests 32/32 (23.03 s), workbench 10 и platform 12 tests, diff-check.
У spPalette подтверждены factory 4B2C80, allocation 414, record 763DE0,
ID 591C0B9F, field10=-1 и constructor-uninitialized 1024 bytes. Полный класс
и renderer ownership пока не закрыты. Бюджеты не повышались, cap retries нет.

Immutable manifest16: 19 imports, 139 independent assessments, 296 links,
FK clean; legacy 31/124 сохранены. PC all 9.13096862%, engine 20.34346505%,
101 assessed; game UNRATED. Старый direct37 = 59.05405405% без изменений и
не является знаменателем цели. PS2 all 1.66666667%, engine 4.12727273%,
direct37 23.64864865%, game UNRATED — без новых оценок.

Документация: docs/research/native-pc-texture-native-source.md; canonical class
cards, index, unknowns и workbench обновлены. Native-data writer, cross/external,
missing mips, palette/material links, errors/reinit/clone и live backend остаются.
Исследование продолжается; это сохранение доказательств, не остановка цели.

## Checkpoint 17 — палитра, владение и реальные ошибки runtime texture codec

19:24 UTC. Восстановлен PC spPalette (591C0B9F, Base,414), source/ABI и runtime
palette codec. Copy constructor копирует1024 bytes с новым index-1; virtual clone
через actual clone-map вызывает no-op copy и остаётся пустым. Первая гипотеза
про пустой copy constructor отвергнута оригинальными байтами. Clone fixture
первоначально не вызвал static-map startup; после actual52FD90/6D7DB0 работает.
Это не cap retry; guards не менялись.

Renderer4BB7D0/4BB840: новый/reused index, native free-list, точные1024 bytes
на COM slot11C; HRESULT игнорируется. Texture setter4B93C0 unregister-ит НОВЫЙ
argument, удаляет old; alias оставляет dangling pointer. DX destructor оставляет
palette allocation/index. Native read/create/write failures имеют документированные
утечки lock/ref и ложные true. Host unique_ptr/strict validation — явное отличие,
не выдуманная native RAII. GPU/OS не запускались; cleanup после наблюдения отдельно.

Проверки: 114 native assertions,17/17 profile,3 exact compiled rows; source813,
fresh build,CTests32/32 (25.93s). Build исправлен под имеющийся C++17 (pointer/size
вместо span), стандарт проекта не менялся. Native-data writer scout дополнительно
прошёл raw/raw2/DXT1-4,12 assertions; это задел следующего checkpoint, не его source.

Immutable manifest17:20 imports,140 latest independent assessments,297 links,
FK clean,legacy31/124. PC all9.26739427%,engine20.64741641%,102 assessed;
gameUNRATED,old direct37=59.05405405% неизменён/не знаменатель цели.
PS2 all1.66666667%,engine4.12727273%,direct37=23.64864865%,gameUNRATED без изменений.
Далее writer native-data, shared material/controller graph, conversion/missing mips,
external sources и полный resource-to-render/Save. Цель активна.

## Checkpoint18 — native TextureData writer в общем ядре

19:38 UTC. Original42BF40→42E5F0/source→cross42DD70/native42B9E0 подтверждён
на CPUData, не runtimeDXTexture. Native vector6C memory-order width/rows/stride/ptr;
wire width/stride/rows/pixels. Policies0/2 дают platform7+cross+native; policy1
platform6+native. Partial4x4 top-level-only writer не генерирует отсутствующие mips.

Source использует общий spDataBlockSerializer и shared SourceNone/cross helpers,
CPUData с отдельным native vector и CPU buffer. Caller-input validation явно
не выдаётся за native vector construction. Полные source outputs читаются через
DX reader; partial writer output остаётся explicit missing-mip reader boundary.

Проверки36 native assertions,9/9 profile (6 new exact+3CPU regressions),source926,
fresh build,CTests32/32(18.90s),Workbench10/platform12/diff-check. Guards прежние,
no cap/OS/GPU/assets/publication. Manifest18:21 imports,140 assessments,298 links,
FK clean,legacy31/124; PC all9.28103683%,engine20.67781155%,102 assessed,
gameUNRATED. Old direct37=59.05405405% неизменён/не знаменатель цели.
PS2 all1.66666667%,engine4.12727273%,direct37=23.64864865%,gameUNRATED unchanged.

Следующая PC-разведка обнаружила старую ошибку labels MaterialTexture: UV
на38 (field12,1C0053D6,4779F6→467D90), AnimTex на64 (field11,16FB0E47,
47799D→476680). Это надо исправить в canonical ABI/docs по PC evidence,
не переносить автоматически на PS2. Далее canonical material-texture references.
Цель продолжается, checkpoint не является завершением.

## Аудит процентов по запросу пользователя — отдельная задача учёта

6 сентября, 20:22 UTC. Пользователь остановил реверс, запросил текущий процент,
затем потребовал пересчитать все показатели для понятного условия завершения.
Новый реверс, CP19 source completion, игра/GPU и публикация в этом аудите не выполнялись.

Найдена неполная миграция старых карточек в independent platform ledger, включая
spBaseObject, streams, buffers, serializers и game bootstrap. По существующим
платформенным доказательствам импортирован immutable
research/native-platform-accounting-audit-2026-09-06.json: 182 reviewed_existing
assessments (75 PC, 107 PS2). Все прежние PC class scores сохранены; часть PS2
wire baselines уточнена по старым отдельным native карточкам, не по PC тестам.
Повышение общего зачёта — исправление учёта, не новые открытия.

Текущие PC: all 15.44747613% (177/733 assessed), engine 34.14285714%
(175/329), game 0.22277228% (2/404), legacy direct37 59.05405405% (37/37).
PS2: all 11.36563877% (137/681), engine 27.74545455% (135/275),
game 0.27093596% (2/406), direct37 31.75675676% (35/37).
Неоценённая история игры не становится нулевым знанием; прежний mixed2.5 не
переносится автоматически на две платформы.

Новый scope research/native-goal-smo-san-scope-v1.json: 308 distinct RTTI classes
в объединении; 276 PC / 245 PS2, в 13 группах, без дублей. Включены все direct37,
все workbench classes и консервативный фронт связанных семейств/candidates.
Это versioned workflow envelope, не доказанный полный call graph. Шесть
non-RTTI obligations не превращены в придуманные классы; они закреплены за gates.
PC workflow **112.38/276 = 40.71739130%**, 175 assessed, 101 unrated.
PS2 **76.35/245 = 31.16326531%**, 135 assessed, 110 unrated.

Условие остановки: все необходимые PC classes closed/100 И все семь обязательных
gates passed; сейчас 0/7 полностью закрыты, 6 partial. Это не нулевой объём работы
и не 6/7 готовности. Неизвестные необходимые тела не скрываются за средним;
неизвестные исходные имена сами по себе не блокируют доказанное поведение.
Scope change требует новой версии и явной причины. PS2 не является stop gate PC.

research/native_goal_coverage.py report пересчитывает all/engine/game/direct37/
workflow из текущих assessments без сканирования EXE/assets. record сохраняет
idempotent snapshot; input fingerprint включает каталог, scopes и assessments,
отдельный assessmentLedgerSha256 отличает новые оценки от смены знаменателя.
Добавлены native_goal_scopes/members/snapshots:1/308/2. Первый provisional
snapshot оставлен в истории; второй использует полный accounting-input fingerprint.
Platform imports22, assessments314; старые research imports31/snapshots124 сохранены.

Проверки: goal accounting14/14, platform12/12, workbench10/10, legacy accounting6/6;
diff-check без ошибок. Независимая арифметика/дедупликация/суммы групп совпали;
FK именно accounting tables чистые. Широкий read-only FK check всей ресурсной базы
остановлен без записей; заменён адресными проверками accounting tables с5s SQLite
guard. Никакого изменения budgets guest, повтора capped probes или C++ build claims.
CP19 незавершённые material edits сохранены, не собраны и не зачтены.

Документация: docs/research/native-coverage-recalculation-2026-09-06.md;
контракт, workbench guide и docs index обновлены. Пользователю передан пересчёт;
этот аудит не является достижением активной цели100% PC SMO/SAN.
