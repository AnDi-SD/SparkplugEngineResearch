# PC: partition graph и статические render supports

Checkpoint12, **2026-09-06 02:16:39 UTC /05:16:39 МСК**. Продолжение
[Scene/world](native-pc-scene-world.md) и
[renderer queues](native-pc-renderer-protocol.md), не новая несвязанная ветка.
Исследуется pristine PC executable с SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

В этом checkpoint восстановлены **ABI и проверенные runtime-контракты**.
Полных portable spatial classes/Scene/Visibility ещё нет. File-layout карточки
остаются доказательством serializer/corpus, не полноты поведения в памяти.
Оригинальные имена классов доказаны; implementation/header paths не найдены.
Найдены exact `Z:\Sparkplug\Code\Sparkplug\spPartition*Serializer.cpp`,
`spZoneSerializer.cpp`, `spZonePortalNodeSerializer.cpp`; переносить эти имена
на неизвестные runtime TU без отметки inferred нельзя.

## Реальные классы, factory и ABI

Все размеры ниже hexadecimal, из actual allocation request, не guessed extent.

| Класс | ID | Factory | Size | Primary/support vtable |
|---|---|---|---|---|
| spPartitionNode |67672341|426910|84|6DCB08,33 slots|
| spZone |61254AB3|480FD0|C8|6EBACC,14 slots|
| spPartitionSystem |912CC341|48E7C0|1D8|6EC528 /6EC510|
| spStaticRenderObject |56D67170|41A7C0|10C|6E65E8 /6E6604|
| spPartitionRenderable |94BBCA2A|4CD950|8C concrete PC|base factory returns derived|
| spPCPartitionRenderable |9CBB56A2|4CD950|8C|6F4540 /6F4528|

`spPartitionNode` derives from BaseObject, **не** spNode. Zone derives from
Node. PartitionSystem физически вызывает RenderNode constructor/copy/world/
destructor, содержит его1D4 prefix и support+B4. Но native RTTI прямой parent
**spNode695C0F65**, не RenderNode603625D0: actual IsKindOf(RenderNode)=false.
Serializer также наследует RenderNodeSerializer. Все три факта совместимы:
нельзя подменять runtime RTTI физическим C++/serializer наследованием.

StaticRenderObject derives NamedObject. PartitionRenderable derives BaseObject;
зарегистрированная base factory возвращает concrete PCPartitionRenderable,
IsExactly(base)=false, IsKindOf(base)=true. Primary tables здесь всего7 entries;
следующая adjacent table **не** дополнительные virtual methods этого класса.

## Владение пространственным деревом

PartitionNode84:

- 10/20/40 — compiler16 vectors borrowed CollisionInfo/RenderNode/
  OcclusionVolume с обратными регистрациями; allocator words untouched.
- 30 — vector intrusive StaticRenderObject refs; duplicate retains разрешены.
- 50 DebugColor=FFFFFFFF;54 borrowed parent;58 owned pointer array;5C count.
  Ненулевые children **direct deleted**, независимо от их intrusive refcount.
- 60 intrusive Zone;64 intrusive ZonePortal vector;74 borrowed PartitionSystem.
- 78 **direct-owned PartitionRenderable**, не StaticRenderObject и не
  intrusive ref.7C=0, точная роль неизвестна;80 borrowed Scene.

Destructor4264D0/deleting426890: notifying drains collisions/render/occlusion,
direct delete78 и children, release portals/Zone/static refs, free vectors,
Base destructor. Own clone426970 использует Base copy40ECE0: собственные
spatial fields, color, lists и relationships **не копируются**.

ZoneC8: NodeB4 + vectorB4{allocator,B8begin,BCend,C0capacity};C4 untouched.
481080→406A50→4255F0 append допускает дубликаты и не берёт refs. Тот же helper
используется Occlusion callbacks с совместимым vector offset; его не следует
объявлять исключительно original `Zone::AddRoot` по одному caller.
Destructor480F00 просто frees vector, **не deletes/releases roots**.
Clone481030 использует Node421F80: own root list пуст, C4 constructor state.

PartitionSystem root1D4 напрямую owned; ctor Node flags70E00 добавляет400 к
обычному RenderNode70A00. Dtor48E6A0 deleting48E870 direct-deletes root и идёт
в RenderNode425050. Clone48E820 использует RenderNode424980, но root остаётся0.
World48E710 — jump4250F0, regular draw424B60 (не SkyBox no-op).

Из decoded SceneInit45D850 восстанавливается ownership без цикла:

```text
Scene3C --intrusive--> PartitionSystem fallback
                      |-- Node child ref --> Zone
                      `-- direct root --> PartitionNode
                                          `-- intrusive60 --> Zone
Zone.localRoots --borrowed--> PartitionNode
```

Actual factories/lifetime graph исполнены; whole SceneInit ещё **не** исполнен:
typed attachment требует VisibilityManager. Literal graph assignments в probe
явно отделены от проверки original setter/loader.

## Обратные регистрации и reset

426690 сначала425660 добавляет root в RenderNode callbacks1C4, затем добавляет
Node в root20. Повторы сохраняются, intrusive refcounts не меняются.
425B60 removes first match swap-last, shrinks **до** вызова424D60(node,root,0).
Обратное удаление424D60 вызывает root virtual2C(root,node,0). Actual destructor
с любой стороны снимает соответствующие регистрации, не удаляя другой объект.
Соответствующие collision/occlusion пары426670/425A80 и4266B0/425C40 найдены
статически; их nonempty concrete lifecycle пока не приписан этому тесту.

Root virtual80=4269C0 освобождает собственные dynamic vectors10/20/40 и portal
vector64 **без** уведомления обратных владельцев. Это не безопасный универсальный
Clear. Actual caller48E790 идёт после48E940: перенос сначала вызывает
464FD0/424DD0/46DCF0 на объектах, затем newRoot virtual1C/28/34 и рекурсирует
children. Цикл полагается на реальное удаление первой записи callback-ом.
Вызов raw reset с живыми reverse links оставляет dangling registrations:
scout это показал; постоянный probe перед teardown восстанавливает намеренно
нарушенный graph через actual non-notifying callback clear. Лимиты не повышены.

4259E0 sets root80, payload78->88, each nonnull static30 entry->88 и recursively
nonnull children. Теперь C8 light-manager payload78 имеет точное original имя
`spPartitionRenderable`. Static/partition objects действительно имеют Scene88,
но разные complete-object/support offsets; их нельзя взаимозаменять по pointer.

## Общий render support и две матрицы

Общая embedded implementation занимает74 байта, original type name неизвестен.
ABI `spRenderSupportObservedLayout` — **analytical name**, не новый найденный
класс. Она находится по RenderNode+B4, Static+14, PartitionRenderable+10.
Relative fields: owned renderable vector4, localSphere14/worldSphere24,
matrix pointers34/38, light cache3C(size28), visibilityMark64, unknown68,
controls6C..6F, complete-object pointer70.

Static complete fields88 Scene,8C matrix,CC inverse. Partition has84 DebugColor
FF000000,88 Scene, обе matrix pointers→shared760058. Static first control=0,
Partition=1; remaining three=1. Common append469ED0 retains every occurrence
и немедленно recomputes local/world spheres через common469820.

**Обязательный startup:**6D38C0 calls461C10(760058,1), создавая Matrix4 identity.
Static ctor копирует её в **две собственные** матрицы; Partition хранит shared
pointers. Без этого initializer cold PE probe видел zeros, но это **не default
полностью запущенной игры**. Поздний startup не исправляет уже скопированные
Static matrices. Native Node identity6D38E0 — другой independent global.

Static copy413120 только Named; Partition copy40ECE0 только Base. Их actual
clones не копируют renderable vectors, matrices/scene/debug color. Shared
Model ownership в двух supports и её teardown проверены оригинальным кодом.

Static prepare44FBA0(support) publishes light cache before device call если
!rendererC9C4; secondary renderer slot38 получает **ровно** complete8C/CC.
На false returns false без публикации sphere; success publishes cached sphere
в rendererC9C8. Partition4D7260 аналогично, но оба args=760058.
Подменённые конечные matrices в probe доказывают отсутствие recomputation
**на границе submission**, не whole-loader принятие произвольного inverse.

Это уточняет [StaticRenderObject file card](smo-class-sp-static-render-object.md):
inverse действительно передаётся downstream независимо, но serializer loading,
upstream изменения и применение backend ещё требуют сквозной трассы.

## Отрисовка не совпадает с RenderNode

44FC00 Static и4D72C0 Partition принимают(camera,force), но не имеют собственного
frustum/Node Enabled gate. Support slot14=4F3DF0 всегда AL true. Slot10 сравнивает
support64 с Scene40; null Scene **не проверяется**, unlike RenderNode.

| Ситуация | RenderNode424B60 | Static44FC00 /Partition4D72C0 |
|---|---|---|
| matrix setup false | прекращает draw | **игнорирует**, продолжает models |
| model draw false | игнорирует, идёт дальше | **останавливается** на первом |
| renderer C050 queue mode | native456310, first failure stop | то же; support pointer adjusted |
| local culling/Enabled | есть | отсутствует в этих двух support bodies |

Такой контракт подтверждён actual Models плюс recording renderer matrix/fog/
mesh leaves; original general vector append выполняется. Это **не** проверка
GPU изображения. Деструкторы clears renderer current-light cache только при
совпадении указателя и !C9C4, затем common support releases model refs.

## Проверки и продолжение

```powershell
python research/probe_pc_partition_runtime.py
python research/probe_pc_static_render_runtime.py
python research/inspect_pc_partition_runtime.py
```

Результаты: **38/38 spatial**, **38/38 static/partition render**, **30/30 static
anchors**. Все normal tracked allocations освобождены actual native teardown.
Guest100k instructions/2 seconds per entry, bounded child30 seconds; no OS/GPU/
game. Source change здесь — byte-exact PC ABI, не complete portable classes.

Остаются P0: Visibility traversal/portal clipping/octree queries, whole SceneInit
и Scene45EC70, serializer→static-matrices trace, portable spatial classes.
Root query slots40..5C, debug60..7C/child geometry, root7C, ZoneC4 и original
support name не объявляются полностью закрытыми. Concrete factory46D1C0
`spVisibilityManager3D7F4387`, global75E1B0, exact allocationAC выявлены;
whole ctor scout достиг прежнего100k cap. Scene global75DB7C —
`spDXShadowVolumeManager04680BC1` (factory4A9030,size3C), actual ctor отдельно
успешен; shader/device render chain ещё не исполнена. Эти обязательные зависимости
не подменяются фиктивными successful managers. PS2 deferred.
