# PC `spLightManager` и сценовый отбор света

Checkpoint 8 цикла до 10:00 МСК, 2026-09-06. Подтверждены original PC
factory/lifetime, borrowed light list, selection/cache algorithms и их вызовы
из scene attachment и world update. Переносимый класс содержит list/selection
срез; это **не полное подключение света к portable Scene/renderer**.

PC SHA-256: `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Новых PS2 исследований в этом checkpoint нет. Исходные названия методов и
путь `Code/Sparkplug/spLightManager.*` не найдены: путь inferred, новые API
помечены `ForAnalysis`, имя самого класса доказано native RTTI.

## Identity, allocation и ownership

| Элемент | PC evidence |
|---|---|
| Class/base ID | `6FCD243A / spBaseObject415352A1` |
| Registry / initializer CALL | `760580 / 6D3BA0` |
| Factory / resolved entry / exact size | `46AB80 → 13BBBD0 / 24` |
| Primary vtable, 7 slots | `6E8CA4` |
| Destructor / deleting | `46A7C0 / 46A830` |
| Clone / getter | `46ABF0 / 46A7D0` |
| Borrowed light append / unlink | `45A780 → 4A9040 / 45A6B0 → 13E1B60` |

Layout: Base10, firstLight10, lastLight14, count18, renderNodeList1C,
ownerScene20. Constructor очищает10/14/18/1C, но **не пишет20**. Сцена явно
записывает owner20 при своём constructor; `45D850` позднее связывает1C с
`&scene18`. Нельзя приписывать standalone manager готовую сцену или нулевой
owner на основании clean heap: native allocation probe заполняет bytes `CC`.

Список невладеющий, links находятся в `spLight` B8(previous)/BC(next).
Parent node hierarchy отдельно держит intrusive ref; manager append не
добавляет ref. Original destructor не освобождает borrowed lights. Clone
создаёт пустой manager, регистрирует pair и вызывает inherited Base copy;
список и owner-состояние не клонируются. Clone-pair call в probe — явный
recording seam, не утверждение о полном native root-clone transaction.

## Scene attachment и world refresh

`45A970/45AB30` проходят actual IsKindOf(Light), добавляют/снимают light в
соответствующей scene34. Original Node Attach/reparent проверен на двух
сценах, трёх lights и render node; middle removal сохраняет порядок списка,
subtree перенос меняет оба membership lists. До unlink `46AA80 → 481140`
снимает light из всех связанных render-node caches через `490BA0`; затем
обходит partition payloads, если owner Scene/PartitionSystem/PartitionNode root
существуют (точные классы установлены в checkpoint12).

`428C30` сначала выполняет Node world421420, затем проверяет
`(currentNodeFlags | inheritedFlags) & 8`, снимает собственный8 и вызывает
scene34 manager `46ACE0 → 46AC60`. Последний проходит render-node list и
добавляет/удаляет изменённый light по текущей eligibility; затем аналогично
обновляет partition subtree. Только dirty1 без8 не обещает light refresh:
upstream property-setter invalidation ещё требует отдельного доказательства.

`46AC40(renderNode)` сбрасывает count114 и ambient110, затем `46AA40`
проходит lights в порядке manager-list и заново отбирает cache. Unused slots
не очищаются. Actual SceneManager45A7D0 → RenderNode world4250F0 → light
selection исполнен; это не ручная подстановка готового cache результата.

Node helper `420DE0(bool)` рекурсивно переключает flag100, независимо от
Enabled200. Он не отсоединяет узлы, не меняет scene links/lists и не обновляет
кэши сам. Portable Node получил именно этот узкий helper. После flag-only
изменения следующий явный native rebuild меняет выбор света.

## Eligibility

`46A850(light, renderNode)` требует light.enabledED и Node bit100; Node
Enabled200 **не проверяется**. Если renderNode121 и light.projectShadowEC,
источник отклоняется. Иначе type0 Directional и3 Ambient проходят без range
test; остальные типы, включая проверенный literal4, проходят при
`distanceSquared(lightWorldPosition74, worldSphereD8) <= (rangeE0 + radiusE4)^2`.
Касание принимается. Point/Spot cone angles здесь не участвуют; это coarse
selection, не доказательство конечной shader/backend формулы освещения.

`46A950(light, payload)` аналогичен, но всегда отвергает projectShadow
и берёт sphere из payload34. [Checkpoint12](native-pc-partition-runtime.md)
установил exact original type **spPartitionRenderable** (concrete PC8C),
не `spStaticRenderObject`, у которого sphere38 и другой support offset.

## Helper cache: восемь обычных и один ambient

Исходное имя helper-типа неизвестно. ABI `spLightCacheObservedLayout` и host
`CacheForAnalysis` — аналитические обозначения, не восстановленные символы.
Размер28: восемь pointers0..1C, ambient20, count24.

- `490B20` очищает все28 bytes.
- `490B50 → 441210` хранит первый ambient отдельно. Обычные pointers идут в
  порядке добавления, duplicates игнорируются, capacity8. Protected constant
  `13B3688` подтверждён после первой безопасной original call, до ninth add.
- `490BA0`: ordinary middle removal заменяет pointer последним и оставляет
  unused tail stale; removal последнего очищает tail. Ambient удаляется только
  при точном совпадении. Cache не владеет объектами и не меняет refs.
- `46AC40` resetSelection очищает count/ambient, но сохраняет raw slots.

Add/remove выбирают ветку по **текущему type** самого light. Восстановление
не обещает автоматической миграции между ambient/ordinary при изменении type.
Повреждённый native count>8 не исполнялся: original guard сравнивает equality,
а не обеспечивает общее восстановление повреждённого контейнера.

## Partition и debug — границы, а не скрытые готовые подсистемы

`46AB20(record, light)` берёт payload78, применяет46A950 и cache payload4C,
затем рекурсивно проходит children58/count5C. `46A7E0` тем же обходом удаляет
light без eligibility. Bounded probe исполнил их на literal root/двух children,
включая отсутствующий root payload. Позднейший checkpoint12 отдельно исполнил
concrete partition/payload constructors/lifetime; этот C8 light probe по-прежнему
literal. Сквозной SMO load/Visibility/light initialization пока не закрыт.

`46AAF0(camera)` вызывает light virtual38 для всего manager-list без внешнего
enabled/100 фильтра. Actual directional virtual428DD0 прошёл в native probe
без GPU. Статический point/spot путь внутри428DD0 использует DebugManager,
sphere helper472450 и debug overlay41DA70; его graphics не исполнялась и в
portable manager не моделируется как готовый renderer.

## Light/LightData: закрытые прежние неизвестные

Original41A330 выделяет **exactF0**. LightData primary6DE990 имеет15 slots,
поддержка6DE98C; padding и opaqueDC остаются untouched. Native copy
`428EB0 → 505CB0` независимо подтвердил прежний PS2 результат: type/color/
attenuation/opaque/range/angles/shadow/enabled копируются, **intensityD8 нет**.
Existing destination intensity9.5 сохраняется при source3.25; fresh actual
LightData clone41ACA0 остаётся1.0. Runtime membership links не копируются.

Copy создаёт lazy CloneManager74E060 даже при явном pair seam; fixture
освобождает его original deleting destructor. Untouched opaqueDC в portable
классе безопасно начинается с0 — это host policy, а не native default.

## Перенос и проверка

Новый `spLightManager` воспроизводит borrowed stable list, blank clone,
eligibility, selection reset, add/remove и refresh одного light. Host list
vector заменяет intrusive pointers, guards duplicate/4096 явно host-only.
Нет implicit Scene owner, automatic attach/world/partition/debug wiring.
Полный portable Node world virtual dispatch остаётся открытым.

- `probe_pc_scene_lights.py`: **90/90** original allocation/copy/clone,
  registration/world/eligibility/cache/partition/debug-dispatch checks;
- `inspect_pc_scene_lights.py`: **22/22** PC-only hash/RTTI/vtable/CALL anchors;
- `SparkplugLightManagerTests`: **29/29**; CTest **14/14**;
- `compare_pc_scene_lights.py`: **1800/1800**, **540** original/portable cases,
  включая raw cache history и finite geometry eligibility. Это exact совпадение
  дискретных результатов, не обещание bit-identical промежуточного x87 math.

Common limits100k instructions/2s per call и30s child сохранены. No OS/GPU/game,
assets/apps/PS2 mutation. Normal tracked original allocations освобождены.
Initial fixture corrections: отсутствующий root clone transaction потребовал
явного pair seam; protected capacity проверяется после resolving call; owner20
оказался untouched, не0; lazy CloneManager потребовал normal teardown. Это
исправления проверок после чтения оригинала, не подмена original bytes.

Открыты: original header/API/support type, opaque lightDC, setters/dirty8,
конкретные partition payloads, SceneInit и complete portable scene integration,
backend light upload и full native root-clone transaction.
