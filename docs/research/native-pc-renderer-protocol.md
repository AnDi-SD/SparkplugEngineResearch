# PC Renderable callbacks и очереди renderer

Дата: 2026-09-06, checkpoint11 цикла до10:00 МСК. PC-first, без нового PS2
анализа. PE `local-data/pc-pristine/WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [Model→RenderNode](native-pc-model-render-world.md).

## Callback contract, не guessed names

Original PC group **423E30**: vector44, begin48/end4C/cap50; enabled54.
Post **423EA0**: vector34, begin38/end3C/cap40; enabled55.
Каждая запись8 содержит callback pointer и borrowed user pointer.
Вызов — **cdecl(renderable,camera,support,ordinal,user)**, результат signed32:

- `-1`: stable left-shift erase текущей записи; iterator остаётся на месте;
- `0`: остановить только этот group loop, сохранив запись;
- другое, включая `-2` и `0x100`: перейти к следующей записи.

Ordinal увеличивается после каждого продолженного шага, **включая erase**.
Второй dispatch начинает ordinal0 на уже уплотнённом списке. Удаление не
уменьшает capacity и не уничтожает user objects. Внешняя мутация списка и
произвольная reentry не доказаны безопасными.

Direct callbacks2C/30 — **cdecl(renderable,camera,support)**, проверяется
только **AL**, поэтому `0x100` означает false, в отличие от групп. Group stop
не запрещает direct callback и не является Model draw failure.

## Материал и последствия failure

Pre **423FD0** сначала рассматривает alpha enqueue, затем pre-group/direct.
Если material6C true, rendererC1C4 сохраняется в **одном global byte7400FC**,
после чего rendererC1C4 обнуляется. Post **4240D0** восстанавливает этот byte
**до** post-group/direct. Два interleaved pre перезаписывают общий save slot;
гарантии stack-like restoration нет.

RendererC188 override запрещает замену currentMaterialC18C/rawFieldC194.
Без override туда идут material20 или fallbackC9C0 и raw Renderable28.
Fog24 передаётся secondary slot26; return игнорируется. Model mesh failure
пропускает post и оставляет rendererC1C4 обнулённым. Это проверенное original
поведение, не исправленное молча удобным scope guard.

## Alpha queue454C30 и flush454850

Условия pre enqueue: material20 nonnull, material4C->10 nonzero, model18 true,
renderer44 false, rendererC9D8 true. Native pre **не проверяет material4C на
null** перед чтением10; валидность pass — upstream invariant. Вызов
**454C30(renderer,model,support,camera,priority)**. Return enqueue игнорируется:
даже capacity failure превращается в pre false / успешный Model skip.

454C30 при renderer45 false сразу возвращает1, не читая inputs. Иначе:
читает model sphere virtual2C; копирует **cached world matrix support34**;
426D40 преобразует center, затем4546F0 применяет cached camera viewCC.
Перспектива использует x²+y²+z², camera **byte231** — только z². Это не
serialized Is2D byteC8. Никакого обновления dirty world matrix здесь нет.

| Renderer field | Contract |
|---|---|
|44/45|flush active / alpha enqueue enabled|
|48/4C|unsigned priority bias / count|
|50|fixed2048 records ×24 bytes, заканчиваются передC050|

Record24: `{camera,support,renderable,distanceSquared,u32Priority,byteExactParticle,pad3}`.
Priority складывается с bias48 с unsigned wrap. Flag — **IsExactly
spParticleSystem5AFA1A4F**, не произвольный transparent material. Queue не
повышает references. Capacity2048 возвращает false после вычисления metric,
но до записи record/count.

Comparator **454800**: обычные записи перед exact ParticleSystem; обычные
сначала по **descending unsigned priority**, затем descending distance;
между двумя particle priority игнорируется. Результат всегда ±1, **даже
равные ключи и unordered/NaN дают +1**, нуля нет. CRT tie order не доказан.

Flush454850 вызывает imported qsort(record24,comparator454800), ставит44=1,
проходит queue; при смене support вызывает его slot4, затем renderable24.
**Оба return игнорируются**, соседний одинаковый support не подготавливается
повторно. В конце44=0/count0, stale record bytes сохраняются. Даже пустой
flush вызывает qsort. Native flush probe использует **явный no-op qsort seam
только для заранее упорядоченных inputs**; comparator проверен отдельно.
Это не claim выполнения CRT sort или определения порядка равных ключей.

## General и nine-bucket queues

**456310** при !rendererC050 false до чтения inputs. Global75F8E8 выбирает
ветку; его runtime startup value пока не доказано.

При false general compiler-vector **C054** хранит record20:
`{renderable,support,camera,materialKey,mesh}`. Material key по цепочке
material4C→18→10→virtual1C; промежуточные null4C/18 дают0. Это отдельный key,
не просто material pointer. Конкретное имя owner/interface ещё неизвестно.
Model или derived даёт mesh58; другие renderables —0. **455EE0** append
исполнен, references не меняются.

**456910** normal path запускает original **456090** sort с predicate454970:
ascending unsigned materialKey, затем mesh address. При изменении support —
slot4, затем renderable24(camera,support); failures игнорируются. **455FE0**
сбрасывает logical count, capacity сохраняется; C050 **не** сбрасывается.
**456B10→456AC0** отдельно освобождает general vector.

При global75F8E8 true support slot14 проверяет Enabled200 у RenderNode;
disabled даёт успешный skip. Mode renderable14 выбирает один из **9** vectors
C064+mode×16; record8 `{renderable,support}`, camera в записи нет. Для native
out-of-range mode guard не найден; probe использует только0/2/8.
PC tables Renderer6E6F78/DX6EFA40/PC6F2918 содержат **5B7A00 no-op** во всех
девяти slots20..40. Flush вызывает семь selected phases20/24/28/3C/40/2C/30
(пять optional globals740240..244 и две unconditional), но в этой PC сборке
они не рисуют. Bucket contents flush не очищает: Scene очищает их в начале
следующего queue pass. Это нельзя выдавать за работающий альтернативный
PC renderer; PS2 classifier0..8 автоматически сюда не переносится.

## Исходники и проверки

`Code/Sparkplug/spRenderable.*`: raw field28 copy, обе callback groups/direct,
stable erase/ordinal/full-word versus low-byte semantics. Original setter
names не найдены, добавленный facade явно `ForAnalysis`. Null/4096/reentry/
vector-mutation guards — **host safety divergence**. Cold field28=0 — не
native constructor palette default; DebugManager sequence сюда не подключён.
Callback copy exclusions сохранены. Это callback phase, не целый pre/post draw.

`Analysis/PC/spRendererQueueMath.h`: finite affine cached-world/view metric,
priority wrap, alpha comparator и general predicate. Renderer queue ownership,
Scene wiring, CRT sort и GPU не симулируются этим helper-ом. PC ABI дополнен
record8/20/24 и точными offsets renderer; прежний общий extentCA08 неизменен.

- `probe_pc_renderable_callbacks.py`: **75/75** original checks;
- `probe_pc_renderer_queues.py`: **64/64** original checks;
- `inspect_pc_renderer_protocol.py`: **28/28** PC SHA/RTTI/table/call anchors;
- `SparkplugRendererProtocolTests`: **23/23**; **CTest15/15**;
- `compare_pc_renderer_protocol.py`: **768/768**, из них192 fields/64 finite
  alpha-key cases и576 fields/48 двухшаговых callback-state cases. Math
  tolerance3e-5, discrete callbacks exact; не bit-identical x87 claim.

Обычные probes освобождают все tracked allocations. Полный PCRenderer
factory scout упёрся в общий100k instructions/2s cap внутри protection;
guest отброшен, полный constructor/teardown **не объявлен выполненным**.
Открытые bridges:4C5A10→443480→4AE370→13D4A50→4563F0(common ctor).
Лимиты не повышались; apps/assets/игра/GPU/PS2/publication не трогались.

Дальше: Scene45EC70 и SceneInit45D850→Partition/Occlusion, source derived
world/Scene wiring, реальные material/backend consumers и runtime default
75F8E8. Не подменять эти существующие original bodies recording successes.
