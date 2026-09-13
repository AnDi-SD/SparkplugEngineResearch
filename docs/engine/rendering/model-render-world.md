# PC: модель, render-узел и точные правила копирования

## Model и Renderable

Original `spModel` factory479ED0→13D09F0 выделяет **0x60**, вызывает
Renderable423F10→13C7260, пишет mesh58=null и projectionGroup5C=**3**.
Это закрывает прежние PC extent/default unknown, независимо от PS2.

| PC entry | Доказанный контракт |
| --- | --- |
| 479D20 | возвращает `mesh+18` или shared zero sphere7601AC; validity не проверяет |
| 479D40 | копирует шесть mesh min/max words2C..40; null mesh даёт нули |
| 479DA0 | нормализует `mesh != null && mesh.byte28 != 0` |
| 479E20 | заменяет intrusive mesh reference; вызывает v34 даже при том же pointer |
| 479E60 | inherited Renderable copy, release old mesh/retain source mesh, copy group |
| 479F40 | fresh factory, pair registration, virtual copy; geometry не deep-clone |

- pre423FD0→fog slot26→mesh slot9→post4240D0;
- false pre означает **успешный skip** на уровне Model; mesh/post не вызываются;
- false fog return игнорируется; false mesh останавливает до post;
- post false возвращается наружу, nonzero нормализуется;
- !rendererC188 публикует material20 либо fallbackC9C0 вC18C и raw word28
  вC194; при override оба cached words сохраняются;
- direct callbacks имеют observed cdecl arguments `(model,camera,support)`.

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

- **412BE0** — always-clone: ++depth14, object v8, --depth; на root exit
  очищает map. Null input возвращает0 до depth/cleanup.
- **412C40→protected→4D3810** — map-aware entry: hit возвращает существующий
  result без allocation; miss вызывает предыдущую операцию.
- **412F70** — register pair: повторный source заменяет mapped result,
  count не растёт; ссылки в map невладеющие.
- RenderNode support вызывает **первый**, не второй entry.

Чистый world pass свет не пересчитывает. Это **не создание Scene**: partition/occlusion notifications, typed registration/reparent, renderer state publication и camera/Light/SkyBox derived world integration ещё открыты. Local matrix preparation не выдаётся за GPU submission. Host detach/clear пересчитывают bounds как ownership facade; отдельный original removal entry ещё не исполнен. Native copy destination append уже исполнен отдельно.

## MeshData/resource dependency

Original MeshData/Resource teardown при отсутствующем cache manager создаёт
ResourceManager458D00 с exact PC allocation**30**, vtable6E703C; actual empty
destructor очищает own singleton75DB78. Таким образом PC size больше не merely
observed extent. Полный nonempty resource-cache/materialization/rollback —
отдельная незакрытая зависимость; borrowed mesh records не считаются GPU meshes.
