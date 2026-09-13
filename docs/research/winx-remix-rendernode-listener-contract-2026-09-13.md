# RTX Remix: RenderNode membership listeners, 13 сентября 2026

Непустой `spRenderNode::callbackBegin..callbackEnd` не является основанием
отклонять ordinary world/material packet. Для подтверждённого partition пути
это **обратные регистрации в spatial partition nodes**. Они не являются Model
pre/post callbacks и не исполняются exact `Prepare4248D0` или `Draw424B60`.
Условие empty в нашем independent adapter было ошибочным предположением о cohort;
восстановленное поведение игры не требовало исправления.

Основание — исполненные ранее [CP7](native-pc-render-node-runtime.md),
[CP12](native-pc-partition-runtime.md) и scene partition-init probe. Дополнительно
read-only проверены десять bounded x86 ranges и шесть slot words в pristine и
debug EXE; все совпадают. Хеши, байты и disassembly:
[evidence JSON](../../research/winx-remix-rendernode-listener-contract-2026-09-13.json).
Новая эмуляция, игра и GPU не запускались.

## Layout и вызовы

| Complete RenderNode offset | Роль |
|---:|---|
| `1C4` | Untouched vector allocator state |
| `1C8` | Begin borrowed listener-object pointers, шаг **4 байта** |
| `1CC` | End |
| `1D0` | Capacity |

Имена существующих ABI fields сохранены; уточнены только комментарии в
[`SparkplugAbi.h`](../../Sparkplug/Analysis/PC/SparkplugAbi.h).
Это не 8-байтовые `{callback,user}` records из `spRenderable`, и элементы не
являются адресами функций. Конкретный доказанный listener — объект partition
node; его original C++ interface name здесь не восстанавливается догадкой.

`426690` получает `ECX=partitionNode`, stack argument `RenderNode*`:
`426698 → 425660` сначала добавляет listener в обратный vector узла, затем
`4266A5 → 426280` добавляет узел в `partitionNode+20`; `426690` завершается
`ret 4`. Entry `425660` является protected thunk; новый static проход не
приписывается декодированию его protected body. Фактические append semantics
проверены предыдущим original probe.

Повторы допустимы, intrusive references не добавляются. Listener может быть
spatial leaf или другой partition node; он не обязан совпадать с верхним
`scene.partitionSystem.root`. Для vtables `6DCB08 / 6E4420 / 6EBA30` slot `28`
равен соответственно `426690 / 449C10 / 480540`, а slot `2C` во всех трёх
равен **`425B60`**.

| Consumer | Подтверждённый ABI / действие |
|---|---|
| `424D60` | `thiscall(node, listener, notify)`, `ret 8`; читает младший byte notify. Первое совпадение заменяет последним pointer, уменьшает end; при notify вызывает `listener.v2C(node,0)` по `424DBD`. |
| `424DD0` | `thiscall(node, notify)`, `ret 4`; true уведомляет с конца через `424E0B`, перечитывает begin/end и уменьшает end; allocation сохраняется. False освобождает и обнуляет vector без уведомлений. |
| `425B60` | `thiscall(partitionNode, node, notify)`, `ret 8`; симметрично удаляет одну запись из partition vector20 и при notify вызывает `424D60(node,partitionNode,0)`. |

Нулевой notify в ответном вызове предотвращает повторную reciprocal notification.
Возврат listener не проверяется как bool. CP7 отдельно демонстрирует, что
произвольный self-removing listener может вызвать дополнительный pop и пропуск
соседней записи: это не универсальная гарантия reentry safety.

## Фаза относительно world witness и rendering

Полный visible `4250F0` сначала сохраняет `node.flags | inheritedFlags` и
вызывает `421420` по `425104`, включая descendants. После base update он
обновляет reciprocal scale; при captured bit1 строит world sphere/помечает lazy
matrix cache и, при наличии scene, завершает ветку вызовом
**`4252BF → 424EF0`** после light-cache refresh. `4250F0` возвращается через
`ret 4` по `4252C9`.

Membership refresh `424EF0` для обычного Enabled узла сцены снимает прежние
registrations через **`424F32 → 424DD0(true)`**, затем вызывает
**`scene.partitionSystem.root.v28(node)`** по `424F4B`. Scene/self-partition/
SkyBox gates согласуются с CP7; protected prefix `424EF3` заново не исполнялся.
Для квалифицированной иерархии этот refresh заканчивается внутри original root
world traversal **до** публикации нашего SystemRoot witness.

Это **не исключительно Update callbacks**. Кроме dirty-world refresh:

- Enabled override `424E70` снимает registrations при выключении по `424E90`
  и регистрирует при включении по `424ECD`, затем вызывает base `421640`.
- RenderNode destructor `425050` уведомляет/drains vector и освобождает его.
- Partition-node destruction и partition-system transfer `48E940` выполняют
  взаимное снятие/перенос; raw partition reset без reciprocal cleanup небезопасен.

Полные exact `Prepare4248D0` и `Draw424B60` не читают этот vector и не вызывают
его listeners. Prepare вызывает affine/inverse helpers `461D70/461EB0`, renderer
matrix slot и публикует sphere. Draw делает Enabled/cull, Prepare либо enqueue
`456310`, затем dispatch renderable `v24(camera,support)`. Поздние **Model**
callbacks остаются отдельной границей [CP10](native-pc-model-render-world.md);
снятие RenderNode empty guard не разрешает Model pre/post callbacks.

## Минимальное применение в independent adapter

Снять только требование **пустого** listener vector. Сохранить его structural
bounds, свежие Enabled/dirty/PRS, известные world-dispatch slots и текущую
иерархию до witnessed `scene+14`, актуальные registry/owner relationships и
финальные mutation/frame/device/update fences. Ничего не вызывать, не очищать
и не воспроизводить из listener vector: оригинальные producers продолжают
работать своим путём ровно один раз.

Непустая обратная регистрация не делает world/material inputs draw-time
producer-dependent. При этом membership не является вечным: последующие
Enabled, transfer, destruction или произвольные material changes сохраняют
необходимость проверки current packet. Этот контракт снимает конкретный ложный
отказ для зарегистрированных ordinary RenderNode, но не объявляет всю
independent submission или произвольные callback implementations проверенными.

Original evidence без повторного запуска:
[`probe_pc_partition_runtime.py`](../../research/probe_pc_partition_runtime.py)
проверяет duplicates, no-ref ownership и взаимное удаление;
[`probe_pc_scene_partition_init.py`](../../research/probe_pc_scene_partition_init.py)
проверяет native manager update → fallback registration и transfer;
[`probe_pc_render_node_callbacks.py`](../../research/probe_pc_render_node_callbacks.py)
проверяет remove/drain/teardown. Общий `spRenderNode.cpp` явно сохраняет неперенесённую
automatic partition ownership boundary; никакая новая игровая реализация здесь
не добавлена.
