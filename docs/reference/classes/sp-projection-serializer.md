# spProjectionSerializer

Общие исходники [spProjectionSerializer](../../../Sparkplug/Code/Sparkplug/spProjectionSerializer.h)
реализуют ограниченный PC reader для родительской Renderable-секции и своей
Projection-секции. Class ID — `0x3344790A`, физический родитель —
`spRenderableSerializer`. Target ID — `0x5CB4145D` (`spProjection`). Точный
исходный header/TU путь неизвестен. Полный класс и PS2-поведение не восстановлены.

На PC factory `4686F0` создаёт объект размером `14` hex с primary table
`6E8494` и secondary table `6E8488` по `+10`. Reader `468820` использует
secondary receiver и сначала вызывает настоящий Renderable reader `47FBA0`.
Каждая из двух последовательных секций заканчивается своим terminator.

| Field | Данные | Назначение в Projection |
| ---: | --- | --- |
| 0 | два raw UInt32 | слова `5C`, затем `60` |
| 1 | два raw UInt32 | слова `64`, затем `68` |
| 2 | byte | zero-extended слово `6C` |
| 3 | byte | raw byte `59` |
| 4 | byte | raw byte `5A` |
| 5 | object reference | заимствованный `spRenderNode*` по `84` |

Слова сохраняются без float-конверсии. Первый элемент пары назначается до
чтения второго; последующий отказ не откатывает уже выполненные назначения.
Byte-поля сохраняют исходные значения без bool-нормализации. Повторы применяются
по порядку; неизвестные поля пропускаются общей DataBlock-операцией.

Field 5 вызывает общий reference resolver с ожидаемым ID `0x603625D0`.
Результат, включая null, заменяет прежний указатель без retain/release.
Смена или очистка ссылки и уничтожение Projection не уничтожают RenderNode.
Внешние владельцы должны сохранять узел живым при последующем использовании.
Сам resolver оригинала не проверяет expected-class аргумент. Portable reader
явно отказывает при ненулевом результате другого физического типа; это
аналитическая граница перед небезопасным назначением, а не native false return.

Общие Renderable- и reference-операции выполняются напрямую, без callback,
подменяющего reader целиком. Проверки размеров/числа полей, I/O и отравление
контекста при отказе — общая аналитическая защита. Квалифицированы корректные
ограниченные scalar chunks и уже разрешённые типизированные FAT relationships;
native malformed-input и startup-семантика из этого не следуют.

Copy — унаследованная literal Base-операция; Clone создаёт отдельный serializer,
регистрирует пару и выполняет этот Copy. Обычные relationship index entry
points без специального PC error-контекста явно отказывают.

PC index `4687D0` сначала обходит Material и Fog родительской Renderable,
затем borrowed RenderNode84. Перед проверкой каждой ссылки общий `467300`
вызывает настоящий ErrorManager gate с аргументами `0,2,0,0`. Даже null
проходит этот gate: общий empty Handle очищает неизвестное raw-слово
`75AB9C` и возвращает false, после чего null reference возвращает true.
Index поэтому не эквивалентен трём пропущенным null-проверкам.

Отдельный `IndexNullRelationshipsWithWarmEmptyPcForAnalysis` допускает только
стабильный живой объект с тремя null relationships, initial empty FAT в
Common/Save context с policy2 и явно переданный живой текущий типизированный
PCErrorManager без очереди, использованных data bytes и fatal/overflow flags.
[Аналитический контекст](../../../Sparkplug/Analysis/Host/spProjectionIndexHost.h)
передаёт менеджеры и внешнее mutable raw-слово; caller явно подтверждает
их стабильное время жизни и отсутствие alias у raw-слова. После admission
операция вызывает общий warm-empty adapter и общий null reference resolver
трижды в исходном порядке. Объект, FAT, represented ErrorManager state и
владение ссылками сохраняются. Проверки domain выполняются до эффектов;
они являются аналитическим отказом, а не native failure semantics.
Cold/lazy acquisition, global ownership/teardown, queued/modal errors,
ненулевой recursive index, произвольный FAT и root save остаются открыты.

Отдельный `IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis` допускает
стабильные ненулевые ссылки на конкретные DXMaterial, Fog и RenderNode,
уже внесённые в переданную FAT. Материал, Fog, Node обрабатываются в том же
порядке, включая null-члены: warm-empty error adapter перед shared reference
index. Native lookup использует actual class ID: для DXMaterial это
`0x797B39EC`, а не ID базового Material. Generic native IndexResource вызывает
FAT Index даже для duplicate, получает false и завершает операцию успешно,
не обходя связи повторно. Shared portable IndexResource использует ранний
выход для существующей записи; native call counts из этого не приписываются
source-реализации.

Новый host-профиль требует явные живые typed serializer bindings, текущий
Common/Save manager с policy2 и ровно тремя регистрациями, чьи текущие lookup
совпадают с этими bindings. Это эквивалентность наблюдаемого dispatch в данном
контексте, а не проверка закрытых регистрационных masks. Допускается хотя бы
одна ссылка; FAT содержит только выбранные отношения с ID7/9/11 для
Material/Fog/Node, nextID равен максимальному выбранному ID плюс один.
Записи имеют правильные actual class/object, fileID0, null name, zero
offset/size и unwritten flag. Фиксированные ID и состояние записей — границы
этого supplied-профиля, не универсальные native требования. Все guards
выполняются до эффектов. Точные portable типы serializer и RenderNode
проверяются host `typeid`, исключая произвольные overrides; DXMaterial и Fog
являются final. Родительский serializer здесь допускает физический Box target.
Время жизни, отсутствие alias у raw-слова и неизменность входов подтверждает
caller. Новый профиль сохраняет represented state и владение, не заявляя
совпадение host ABI или `shared_ptr::use_count` с native intrusive count.
Прежний null-only endpoint и обычные Index entry points сохраняют свои
ограничения. Новые FAT-вставки, recursive graphs, native cleanup, cold startup,
root save, PS2 и аппаратное выполнение этим профилем не квалифицированы.

PC writer `468A50` записывает Renderable-секцию, затем Projection-секцию.
Пары field0/1 пропускаются, только если оба raw слова равны `0x3F800000`.
Каждая непустая пара резервирует Fixed8 header, записывает два UInt32 по
отдельности и повторно записывает header через seek. Field2 пропускается при
целом слове `6C == 2`; иначе записывается только младший byte. Ненулевые
байты `59/5A` записываются без bool-нормализации. Field5 пропускается при
null; иначе использует общий SaveReference и UInt32 size reservation.
Flags58[0], cached sphere и прочее рабочее состояние не сериализуются.

Portable writer допускает стабильный живой объект со ссылками null либо
внешне удерживаемыми конкретными DXMaterial, Fog и RenderNode, уже записанными
в переданной FAT. Все три ссылки проверяются до начала вывода. Общий
Renderable reader восстанавливает владеющие Material/Fog через явные shared
owners контекста; замена освобождает прежнюю ссылку, совпадающий объект
сохраняет число владельцев. Bound84 остаётся заимствованным и не удерживается.
Байты, успешные write/seek и обратное чтение сохраняют исходный порядок.
Для Fixed8 используется общий encoder заголовка и локальная последовательность
patch: общий portable builder намеренно запрещает произвольные fixed-size
reservations. Raw float encodings не вычисляются. Новый inline Node, Material,
Fog, другие material-типы, неподдержанный derived serializer и отсутствие требуемой FAT отвергаются
до записи. При отказе I/O результат следует отбросить; совпадение поведения
оригинала на ошибочных или reentrant streams не заявляется.

Матрицы, draw, заполненные рабочие контейнеры, Node/Scene producer и teardown,
полный ресурсный loader, PS2 и аппаратное выполнение остаются открыты.


Отдельный IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis описывает
первую вставку и повтор для единственной typed Fog при null Material и
RenderNode. Явный host-контекст содержит ровно один FogSerializer binding;
текущий Common/Save manager с policy2 имеет одну соответствующую регистрацию.
Допускается пустая FAT с nextID1 либо ровно та же unwritten Fog-запись ID1 с
nextID2, fileID0, null name и zero offset/size. Fog имеет точный final-тип и
null physical name; её собственный native Index возвращает true без чтения
скалярного payload. Это не квалификация рендеринга или интерпретации Fog data.

Три warm-empty error gate/reference шага сохраняют порядок null Material,
Fog, null Node. Первая Fog reference использует общий FAT insert и общий
typed Fog Index; повтор сохраняет metadata и пропускает own walker. Native
создаёт36-byte save entry, два24-byte map node и12-byte ordered link.
Это native allocation/storage facts, отдельно от переносимых контейнеров,
владения и ABI. Native duplicate FAT call и portable existing-entry early
return также остаются разными реализациями одного ограниченного эффекта.

Точные host receiver/target types проверяются до неизвестных callbacks;
дополнительного GetTarget callback нет. Менеджеры, Fog и raw-слово остаются
живыми и стабильными, raw не alias их storage. Все guards выполняются до
эффектов; они являются host-допуском, а не native failure parity. Прежние
null/preindexed endpoints и shared Find/FAT алгоритмы не расширены. Material
insertion с вложенным error-контекстом, Node insertion, произвольная рекурсия,
allocation failure/rollback, cleanup, root save, PS2 и hardware открыты.


IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis is a separate
explicit profile for one final DXMaterial with NULL physical name, zero passes
and NULL color controller; Fog and Node stay NULL. Its own Host context borrows
one exact MaterialSerializer binding, warm-empty PCErrorManager, nonalias raw
word and current Common/Save policy2 manager with exactly one registration.
FAT is empty/next1 or exactly the same unwritten Material ID1/next2, file0,
NULL name, zero offset/size and matching ID/object maps. All inputs stay live
and stable synchronously; exact type/shape guards precede effects and invoke
no GetTarget callback. Power at native B8 is not read or invented by Index.

An unregistered stack-local ForAnalysis adapter is an explicit source dispatch
substitution. The real typed binding is checked, but shared IndexResource uses
the local adapter's own callback after its unchanged insertion. That callback
qualified-calls the unchanged generic Material tail, pure only for the admitted
empty shape, then executes the additional warm-empty NULL-reference gate.
The declared substitution is pure empty tail followed by nested gate. Native
runs that gate inside actual Material own Index before its NULL-reference
completion; effect order remains post-insertion and before outer NULL Fog/Node.
Repeat's shared existing-entry return skips own dispatch and nested gate. The
adapter is never registered/retained; no mutable current context/global/TLS is
introduced. Its receiver/RTTI/factory/callback counts are not native mappings.

Native first insertion has four error gates, repeat three. Native entry36,
RB24 twice and ordered-link12 are distinct from portable containers/ownership.
Old null/preindexed/Fog-only endpoints and shared Material/Serializer/Find/FAT
algorithms are unchanged. Populated Material/controllers, protected Node walker,
recursive graphs/root save, allocation/failure/rollback/cleanup parity, cold
startup, PS2, hardware and whole-class completion remain outside this profile.
