# PC: модель, render-узел и точные правила копирования

Checkpoint10 цикла до10:00 МСК 2026-09-06. Pristine PC SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [RenderNode](native-pc-render-node-runtime.md) и
[specialized scene managers](native-pc-scene-special-managers.md).
Новые исследования PC-only; исходные class names сохранены, пути/ForAnalysis
API остаются явно inferred, не выдаются за найденные original declarations.

## Model и Renderable

Original `spModel` factory479ED0→13D09F0 выделяет **0x60**, вызывает
Renderable423F10→13C7260, пишет mesh58=null и projectionGroup5C=**3**.
Это закрывает прежние PC extent/default unknown, независимо от PS2.

| PC entry | Доказанный контракт |
|---|---|
|479D20| возвращает `mesh+18` или shared zero sphere7601AC; validity не проверяет |
|479D40| копирует шесть mesh min/max words2C..40; null mesh даёт нули |
|479DA0| нормализует `mesh != null && mesh.byte28 != 0` |
|479E20| заменяет intrusive mesh reference; вызывает v34 даже при том же pointer |
|479E60| inherited Renderable copy, release old mesh/retain source mesh, copy group |
|479F40| fresh factory, pair registration, virtual copy; geometry не deep-clone |

PC Model v34 — **48EAA0 no-op**, а base Renderable v34 —423B60, который
обнуляет14. Поэтому прежнее принудительное обнуление в portable Model было
неточным: теперь виртуальный hook сохраняет именно PC различие. PS2 classifier
`0..8` не перенесён в PC. Source model now delegates sphere/min/max/validity;
base Renderable sphere423B50 равна shared zero, base extents423BF0 вычисляют
center±radius через virtual sphere getter.

Original Renderable constructor берёт word28 из **spDebugManager41D4E0**,
lazy pointer75526C, table73FEE8/sentinel73FEBC. Это **не renderer default и
не доказанный float**. Первый model в данной fixture получаетFF000000;
значение зависит от общего cursor, не универсальная константа каждого объекта.
PC ABI field28 исправлен на raw u32. Полный тип/имя этого значения и его
portable DebugManager/renderer wiring пока не объявлены восстановленными.

PC pre/post protocol исполнен вместе с original Model479DC0, но renderer
mesh/fog leaves и direct callbacks заданы явными recording fixtures:

- pre423FD0→fog slot26→mesh slot9→post4240D0;
- false pre означает **успешный skip** на уровне Model; mesh/post не вызываются;
- false fog return игнорируется; false mesh останавливает до post;
- post false возвращается наружу, nonzero нормализуется;
- !rendererC188 публикует material20 либо fallbackC9C0 вC18C и raw word28
  вC194; при override оба cached words сохраняются;
- direct callbacks имеют observed cdecl arguments `(model,camera,support)`.

Static groups уточнены: byte54 вызывает pre423E30 с vector44, byte55 —
post423EA0 с vector34. Прежние PC ABI имена flags были переставлены;
теперь `usePreCallbacks44`/`usePostCallbacks34`. Nonempty groups, alpha queue,
material-state restore/failure и actual mesh/backend ещё требуют дальнейшего
исполнения/переноса; здесь нет fake-success renderer в исходниках.

## Настоящий SMO append и copy RenderNode

Reader469190 вызывает **469ED0(this=node+B4,renderable)**. Он добавляет
intrusive relationship и сразу вызывает469820: local sphere объединяется,
world sphere строится через **старую cached matrix**, X-radius rule. Node B0
и matrixDirty134 при этом не меняются. Повторный pointer хранится повторно и
получает отдельный ref. Validity Model479DA0 не является скрытым gate.

`424980→13B95D0` сначала копирует Node, затем support
`469FB0→13BF570`, затем cullBypass130. Support copy:

1. записывает source local/world spheres в destination;
2. для **каждого** source renderable вызывает always-clone412BE0 и append469ED0;
3. **добавляет** к имеющемуся destination list, не очищает его;
4. снова записывает source spheres после append recomputation;
5. копирует bytes120..123.

Cached matrices138/178, reciprocal scale1B8, dirty134, word118, light cache и
scene ownership остаются destination state. Даже root clone с настоящей
временной map создаёт **два отдельных Model для двух вхождений одного pointer**.
Они сохраняют общую mesh relationship. Прежняя portable дедупликация
renderables была более общей политикой, но не данным PC алгоритмом, и удалена.
Source copy order/append/spheres/control bytes теперь совпадают с доказательством.
Host self-copy отвергается: native growing self-append не обещает termination.

## Почему clone-map не означает alias reuse в каждом месте

Здесь впервые для этого consumer исполняются actual map initializer52FD90
(точный вызов startup6D14C0, **без atexit**) и destructor6D7DB0.
Global map755588 содержит sentinel pointer75558C/count755590; его allocations,
insert/overwrite/lookup/clear исполняются, а не подменяются dictionary seams.

- **412BE0** — always-clone: ++depth14, object v8, --depth; на root exit
  очищает map. Null input возвращает0 до depth/cleanup.
- **412C40→protected→4D3810** — map-aware entry: hit возвращает существующий
  result без allocation; miss вызывает предыдущую операцию.
- **412F70** — register pair: повторный source заменяет mapped result,
  count не растёт; ссылки в map невладеющие.
- RenderNode support вызывает **первый**, не второй entry.

Прямой вход в4D3810 до public protection resolution оставляет ещё не
разрешённый singleton-address13B342C и падает в guest. Public412C40 штатно
разрешает его в74E060. Соседний4D3800 — вообще другая protected функция,
не альтернативный clone entry. Исправлены **наши точки входа**, не original
EXE/лимиты. Arbitrary cycles, failure rollback и unknown clone callers не закрыты.

## Переносимая CPU-цепочка

`spNode::UpdateWorldForAnalysis` теперь virtual, и plain-parent traversal
действительно вызывает derived `spRenderNode` updater. В source соединены:
Model/mesh sphere→append/union→world PRS→lazy render matrix/inverse→culling.
Сохраняются capture-before-base, billboard next-frame dirty, radius-only reset,
разница cached X-radius/max-scale и destination-cache copy semantics.

Через явную borrowed scene-light dependency подключён existing LightManager
cache rebuild: только captured transform bit1, control123 и Enabled200.
Чистый world pass свет не пересчитывает. Это **не создание Scene**:
partition/occlusion notifications, typed registration/reparent, renderer state
publication и camera/Light/SkyBox derived world integration ещё открыты.
Local matrix preparation не выдаётся за GPU submission. Host detach/clear
пересчитывают bounds как ownership facade; отдельный original removal entry
ещё не исполнен. Native copy destination append уже исполнен отдельно.

## MeshData/resource dependency

Actual MeshData41A270 выделяет58, ставит6DE8FC/6DE8F4, zero sphere/false
validity, но min/max2C..40 и owners50/54 остаются allocator poisonCC. Безопасный
portable nullptr-default остаётся **явной divergence**, не native гарантией.
Перед cold teardown probe явно задаёт owners0; это не доказательство Init.

Original MeshData/Resource teardown при отсутствующем cache manager создаёт
ResourceManager458D00 с exact PC allocation**30**, vtable6E703C; actual empty
destructor очищает own singleton75DB78. Таким образом PC size больше не merely
observed extent. Полный nonempty resource-cache/materialization/rollback —
отдельная незакрытая зависимость; borrowed mesh records не считаются GPU meshes.

## Проверки

- `probe_pc_model_runtime.py`: **47/47** native model/bounds/copy/pre-post и
  MeshData cold constructor/ResourceManager allocation;
- `probe_pc_render_node_ownership.py`: **29/29** native append/copy/root clone,
  real map hit/miss/overwrite/always-clone;
- `inspect_pc_model_runtime.py`: **23/23** PC-only hash/RTTI/table/call anchors;
- `SparkplugRenderNodeTests`: **34/34**, включая derived dispatch и light cache;
- `compare_pc_model_render_world.py`: **2272/2272**,32 whole state sequences;
- прежний `compare_pc_render_node.py`: **1504/1504**,144 math cases;
- CTest **14/14**, single-worker build.

Finite-input comparisons используют absolute/relative tolerance3e-5, не
bit-identical x87 claim. Обычные guest probes освобождают все tracked native
allocations; borrowed payloads/recording device leaves названы явно.100k
instructions/2sec call,30sec process cap; никакого OS/GPU/game/PS2 запуска,
правок приложений/assets или публикации. Проценты обновляются только manifest
и fixed DB denominators, не по числу строк исходников или assertions.
