# Дополнение owner audit: inherited RenderNode support

13 сентября 2026. Собственный контракт адаптера. Причина расширения — текущий
live registry основного агента: supported251/dynamic251/unsupportedDynamic1,
без exact Static/Partition occurrences. Это наблюдение сообщил основной агент;
здесь игра не запускалась. Предыдущие Static/Partition/Model ABI верны:
различается фактический owner cohort.

## Доказанная native граница

Использованы [RenderNode CP7](native-pc-render-node-runtime.md),
[Model CP10](native-pc-model-render-world.md),
[queues CP11](native-pc-renderer-protocol.md) и общий
`Sparkplug/Analysis/PC/SparkplugAbi.h:1205` (`spRenderNodeLayout`, size1D4).
Повторная read-only Capstone проверка сравнила pristine/debug ranges
425050/A0,4255D0/1E,4248D0/E0,6DCAA4/38. Они совпадают; это не выполнение
native/protected functions. Исходные SHA файлов приведены в
[общем контракте](winx-remix-owner-hook-contract-2026-09-13.md).

| Граница | Точный контракт |
|---|---|
| Support table | `6DCADC`: шесть entries `424B60,4248D0,424C30,425040,424790,4247B0` |
| Draw hook | word `6DCADC`, slot0 → `424B60`; ECX=complete+B4; stack `(camera,forceVisible)`, AL boolean, ret8 |
| Prepare | word `6DCAE0`, slot1 / offset4 → `4248D0`; ECX=adjusted support, no stack args, AL boolean, plain ret |
| Model call | immediate `424C1F: call [edx+24]`; pushes support, then camera; queued Model receives те же arguments |
| Queue enqueue | `424BD6→456310(renderable,support,camera)` |
| Exact primary | `6DCAA4`, 14 entries; **не обязательная primary identity для inherited cohort** |
| Complete self | `support+70 == complete+124 == complete == support-B4` |
| Scene | `complete+3C`, inherited Node field; это не Static/Partition complete+88 |
| World pointers | `support+34 == complete+E8 == complete+138`; inverse `support+38 == complete+EC == complete+178` |
| Enabled | Node flags `complete+B0`, bit `200`; соответствует `[support-4]`, shift9/test1 в `424B63..424B6B` |
| Dirty cache | `complete+134`, bit1; world/inverse inline138/178, reciprocal scale1B8 |

Prepare при dirty строит inline matrices, снимает dirty **до** backend call.
`42494C` посылает renderer secondary+18 slot14 (`+38`) pointers
`support+84 = complete+138` и `support+C4 = complete+178`.
False renderer result возвращается без sphere publication; dirty уже снят.
Original Draw этот false уважает и mesh не вызывает. Поэтому owner-аудит не
должен сам вызывать Prepare или исправлять dirty cache.

В Draw forceVisible пропускает только frustum test; Enabled проверяется всегда.
Однако general/alpha flush вызывает Model напрямую, не повторяя RenderNode
Draw/Enabled. Изменившийся после enqueue Enabled сам по себе не доказывает,
что уже исполненный queued Model ошибочен. Записывать это поле как наблюдение
можно; дополнительный adapter reject будет консервативным ограничением, а не
восстановлением отсутствующей native проверки.

Аргументы Model подтверждены `479DC1/479DC7` и flush:
alpha `45489B`, general CP11; slot24 вызывается как `(camera,adjusted support)`.
Draw/Prepare не перепутаны. Собственный Model→native mesh callsite остаётся
`479DF0`, return `479DF3`; менять его guard не требуется.

## Generic owner qualification

Сохранять exact support table `6DCADC`, arithmetic self relation, scene и
живое членство в partition node vector20. Это позволяет принимать derived
primary classes с тем же inherited support без списка моделей или class IDs.
Не вызывать primary RTTI/getters/factories ради аудита. Если primary сохраняется
как дополнительный identity guard, читать его guarded и сравнивать с текущим;
одно совпадение opaque primary не доказывает membership или object generation.

Для transfer geometry проверять оба inline pointer identities выше, конечный
скопированный world и совпадение с native renderer/D3D world. Поддержка custom
secondary vtable остаётся закрытой. Guarded читаемость объекта плюс exact
support, self, scene и actual root graph membership сильнее одного primary
whitelist; она всё равно не превращает borrowed address в retained object.

`Registry::occurrences` теперь включает `OccurrenceSource::RenderNodeVector=20`:
`node` — partition node, `object` — complete RenderNode, `support=object+B4`,
`ordinal` — исходный индекс borrowed vector20, с сохранением null gaps.
Каждое ненулевое повторное вхождение поддержанного object записывается, в том
числе в другом partition node. Повторный обход того же физического node через
Zone/children по-прежнему не дублирует его records. `dynamic`, `supports` и
unsupportedDynamic остаются unique; supports сохраняется sorted для visibility.
Custom dynamic support не получает supported occurrence.

Bound occurrenceLimit65536 общий для всех трёх sources. При overflow
occurrencesComplete=false, прежний visibility walk завершается. Owner consumer
обязан отвергать неполный provenance. Scope cache, mutation serial и реальные
native vectors нужно проверять так же, как для Static/Partition.

## Lifecycle hook

Common direct RenderNode dtor **425050..4250E4** —
`void __thiscall(void* complete)`, no explicit arguments, plain ret.
Перед original вызвать только adapter retirement/invalidation; после него
borrowed vectors/links больше не читать. Body устанавливает primary6DCAA4 и
support6DCADC, вызывает `424DD0(1)` для callbacks, освобождает callback vector,
затем support469C80 и Node422150.

Safe whole displaced prefix **7 bytes**:
`6A FF 68 3F FC 6B 00` = `push -1; push 6BFC3F`; resume **425057** до чтения
FS:[0]. Prefix не содержит relative transfers. Это тот же SEH trampoline
шаблон, что у Scene/PartitionNode; не разрезать второй push на пятом byte.

Deleting wrapper `4255D0..4255ED` принимает flags, вызывает425050, при flags&1
освобождает this, возвращает this в EAX и делает ret4. Word6DCAA4 slot0
перехватывает только exact primary; derived deleting tables отличаются.
Для общей inherited lifetime инвалидации подходит direct425050, без отдельного
повторного callback из каждого deleting wrapper. Конкретные все derived
destructor-to-base paths здесь заново не исследовались.

Изменения реализации ограничены `winx_scene_geometry.h` и его CPU fixture
`test_scene_geometry.cpp`. Owner hooks и запуск принадлежат основному агенту;
проверочная сборка не является live owner PASS.
