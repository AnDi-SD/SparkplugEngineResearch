# PC Renderable callbacks и очереди renderer

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

## Материал и последствия failure

Pre **423FD0** сначала рассматривает alpha enqueue, затем pre-group/direct.
Если material6C true, rendererC1C4 сохраняется в **одном global byte7400FC**,
после чего rendererC1C4 обнуляется. Post **4240D0** восстанавливает этот byte
**до** post-group/direct. Два interleaved pre перезаписывают общий save slot;
гарантии stack-like restoration нет.

## Alpha queue454C30 и flush454850

454C30 при renderer45 false сразу возвращает1, не читая inputs. Иначе:
читает model sphere virtual2C; копирует **cached world matrix support34**;
426D40 преобразует center, затем4546F0 применяет cached camera viewCC.
Перспектива использует x²+y²+z², camera **byte231** — только z². Это не
serialized Is2D byteC8. Никакого обновления dirty world matrix здесь нет.

| Renderer field | Contract |
| --- | --- |
| 44/45 | flush active / alpha enqueue enabled |
| 48/4C | unsigned priority bias / count |
| 50 | fixed2048 records ×24 bytes, заканчиваются передC050 |

Record24: `{camera,support,renderable,distanceSquared,u32Priority,byteExactParticle,pad3}`.
Priority складывается с bias48 с unsigned wrap. Flag — **IsExactly
spParticleSystem5AFA1A4F**, не произвольный transparent material. Queue не
повышает references. Capacity2048 возвращает false после вычисления metric,
но до записи record/count.

Comparator **454800**: обычные записи перед exact ParticleSystem; обычные
сначала по **descending unsigned priority**, затем descending distance;
между двумя particle priority игнорируется. Результат всегда ±1, **даже
равные ключи и unordered/NaN дают +1**, нуля нет. CRT tie order не доказан.

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

Дальше: Scene45EC70 и SceneInit45D850→Partition/Occlusion, source derived
world/Scene wiring, реальные material/backend consumers и runtime default
75F8E8. Не подменять эти существующие original bodies recording successes.
