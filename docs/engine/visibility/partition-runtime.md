# PC: partition graph и статические render supports

## Реальные классы, factory и ABI

Все размеры ниже hexadecimal, из actual allocation request, не guessed extent.

| Класс | ID | Size |
| --- | --- | --- |
| spPartitionNode | 67672341 | 84 |
| spZone | 61254AB3 | C8 |
| spPartitionSystem | 912CC341 | 1D8 |
| spStaticRenderObject | 56D67170 | 10C |
| spPartitionRenderable | 94BBCA2A | 8C concrete PC |
| spPCPartitionRenderable | 9CBB56A2 | 8C |

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

```text
Scene3C --intrusive--> PartitionSystem fallback
                      |-- Node child ref --> Zone
                      `-- direct root --> PartitionNode
                                          `-- intrusive60 --> Zone
Zone.localRoots --borrowed--> PartitionNode
```

## Обратные регистрации и reset

426690 сначала425660 добавляет root в RenderNode callbacks1C4, затем добавляет
Node в root20. Повторы сохраняются, intrusive refcounts не меняются.
425B60 removes first match swap-last, shrinks **до** вызова424D60(node,root,0).
Обратное удаление424D60 вызывает root virtual2C(root,node,0). Actual destructor
с любой стороны снимает соответствующие регистрации, не удаляя другой объект.
Соответствующие collision/occlusion пары426670/425A80 и4266B0/425C40 найдены
статически; их nonempty concrete lifecycle пока не приписан этому тесту.

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

Это уточняет [StaticRenderObject file card](../../reference/classes/sp-static-render-object.md):
inverse действительно передаётся downstream независимо, но serializer loading,
upstream изменения и применение backend ещё требуют сквозной трассы.

## Отрисовка не совпадает с RenderNode

| Ситуация | RenderNode424B60 | Static44FC00 /Partition4D72C0 |
| --- | --- | --- |
| matrix setup false | прекращает draw | **игнорирует**, продолжает models |
| model draw false | игнорирует, идёт дальше | **останавливается** на первом |
| renderer C050 queue mode | native456310, first failure stop | то же; support pointer adjusted |
| local culling/Enabled | есть | отсутствует в этих двух support bodies |

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
