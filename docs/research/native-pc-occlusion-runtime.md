# PC `spOcclusionVolume`: геометрия и потребитель в Scene

Checkpoint15, 6 сентября 2026. Контрольный pristine PC executable и прежний
SHA256; игровой процесс не запускался. Original path **подтверждён** строкой
`0x006E8D14`: `Z:\Sparkplug\Code\Sparkplug\spOcclusionVolume.cpp`.
Это именно TU класса, отдельно от известного serializer TU. Header неизвестен.

## Что исполнено, а что ещё нет

Native factory/ctor/dtor, Node-only clone, CPU IB/VB readers и weld,
world transform/reciprocal partition links, построение плоскостей из явно
заданного camera-facing silhouette и **полный SceneRender с непустым occluder**.
Подготовленный силуэт и три CPU-буфера — объявленные входы fixture, не результат
успешного native Init. Renderer/Visibility boundaries унаследованы от C13/C14.

Full `470FE0` после настоящего чтения/копирования буферов и сортировки достиг
**100000 instructions** внутри защищённой geometry dependency (`IP889723`).
Лимиты 100k/2sec/call и30sec/child не повышались; остановленный вызов не
продолжался. Следовательно, **полная загрузка/валидация occluder ещё не доказана**.
Отдельные независимые weld/world/plane tests не выдаются за завершённый Init.

## Identity, layout и lifetime

Original ID `43D24430`, direct `spNode695C0F65`, registration `760640`,
factory `470A70`, exact allocation **1B8**. Primary `6E8DE0`, ровно14 слотов:

```text
46FFC0 420B40 470AD0 421F80 46FD70 408350 408370
46D7E0 46D520 4212F0 420610 421330 46DD90 421640
```

За таблицей уже строковые данные, не дополнительные virtual methods.
Protected ctor `46FE60`/slot`13B324C` и dtor `46FC50`/slot`13B1C28`
исполнены; deleting wrapper `46FFC0`. Восстановленный exact ABI —
`Sparkplug/Analysis/PC/SparkplugAbi.h`, не полная portable class implementation.

| Offset | Подтверждённая роль |
|---|---|
| B4 | borrowed reciprocal PartitionNode vector16; allocator не инициализируется |
| C4 | visibility stamp, начально0 |
| C8 /CC /D0 | direct-owned local VB /world VB /UInt16 IB, начально null |
| D4 /D8 | border-edge count /owned edge-pointer vector16 |
| E8 /F8 /108 | border Vector3 values /face32 values /camera face-plane16 values |
| 118 | cached world camera position; dirty flag1B5 |
| 124..15F | auxiliary geometric state; полные роли остаются открытыми |
| 160 /164 | planar flag /border walk stamp |
| 168 | plane-set20: vector of plane20 +active count |
| 17C /188 | local mins/maxs, Init scan начинает с нулей |
| 194 /1A4 | local /world sphere4 |
| 1B4 /1B5 | initialized /camera geometry dirty; ctorfalse/false |

Face32: plane4, три position pointers, camera-side value `0/1/2`.
Edge28: start/end position pointers, opposite/own face pointers, vector
outgoing edges, walk stamp, border byte. Эти роли дополнительно читаются
readable helpers `46E1A0/46EB10/46FFE0`, но не являются заявлением о полном
исполнении protected topology builder.

Clone `470AD0` создаёт новый1B8, register`412F70`, вызывает inherited
`Node421F80` через v0C. На подготовленном исходном объекте подтверждено:
**собственные буферы/силуэт/init state не копируются**. Имена и дети здесь
пустые; special nonempty-name alias lifetime этим тестом не покрывается.

## CPU geometry preparation

Readable Init`470FE0` сначала вызывает clear`46F880`:
direct deletes IB/VB/worldVB, обнуляет три указателя, удаляет edge objects,
сбрасывает initialized. При этом clear **не стирает cached plane set**.

Далее по static code:

- UInt32 IB branch копирует **младшие16бит** каждого индекса, без локальной
  проверки high bits; UInt16 branch использует deep-copy`45FD90`;
- VB копируется в position-only buffer: component offset24, stride18 в dwords;
- geometry helper`4604F0`, weld`460D90`, затем worldVB copy`460240`;
- protected topology/shape builder`470E30`; failure возвращаетfalse, не
  выполняя повторный полный cleanup собственных промежуточных буферов;
- success: face-plane reserve, local sphere`468370`, bounds scan с началом
  в `(0,0,0)`, dirty/init=1. Это static contract, не успешный full Init test.

Standalone UInt16 weld **исполнен**: пять вершин, включая дубликат, становятся
четырьмя; IB `(0,1,2,4,2,3)` становится `(0,1,2,0,2,3)`. Использованные
вершины после compaction сохраняют порядок исходного VB в этом тесте.
Компаратор`4607F0→13D1850` берёт VB из shared global`75FF9C`, сравнивает
**bytes**, не float distance: `-10` идёт после`+10`, `+0`/`-0` различаются.
General optional component comparisons видны static, исполнен position-only case.

CRT boundary `pc_qsort_u16_fixture.py` — bounded insertion sort для≤64
UInt16 IDs, **каждое сравнение выполняет original PC comparator** обычным
guest CALL. Стабильность tie-order — свойство fixture, не MSVCRT. Original
qsort implementation/равные representative IDs не восстановлены. Unknown
signatures по-прежнему отвергаются; alpha boundary из C11 не расширен.

Reader`44F400` действительно вызывает Init при чтении и проверяетAL;
при failure пишет `Failed to initialize occlusion volume!`. В обеих ветках
временные входные IB/VB удаляются. Поэтому прежний вывод «раз вогнутый mesh
есть в файле, загрузка обязательно принимает его» **отозван**. Нужно исполнить
именно этот authored decagon и проследить caller failure, а не делать convex hull.

Readable shape helpers используют componentwise float epsilon **0.001f**:
`46E1A0` связывает противоположные edges/проверяет внутренний shape,
`46EB10` удаляет coplanar internal edges, `46E3B0` требует coplanar faces при
ненулевом border count и устанавливает planar160. Closed shape имеет planar0.
Это ещё не доказательство полного допуска слегка вогнутого decagon.

## World, camera planes и Scene

`46DD90` запоминает old flags|inherited до base Node world. При bit1:
dirty1B5=1; еслиinitialized, каждый XYZ C8 переносится через Node`420660` вCC.
World sphere center — signed scale→orientation→translation, radius — local
radius×max(abs(world scale)). Реальный тест `(-2,3,4)` иtranslation`(1,2,3)`
проверяет все четыре вершины и сферу. Затем reciprocal unregister/register
через Scene partition root v34. **Enabled200 здесь не gate**.

Унаследованный `421640(enabled,recursive)` не unregister-ит occluder;
actual Scene продолжает учитывать отключённый узел. Reverse drain`46DCF0(true)`
уведомляет root v38, опустошает логический список, сохраняет capacity;
false освобождает его без уведомления. Original teardown освобождает все buffers.

`46FFE0` до plane builder: initializedfalse→false; иначе при !dirty и том же
camera world position использует cache. Rebuild вычисляет local camera,
classifies faces по signed distance `<-0.001`→1, `>+0.001`→0, иначе2;
по edge adjacency выбирает silhouette. Полный nonempty rebuild пока open.

`4702E0` из **явного готового cache** исполнен:

- empty/меньше3 border points → success с нулевыми planes;
- face planes копируются как есть, duplicates не удаляются;
- side planes `471420(camera,next,current)` строятся для соседних пар;
  последняя точка должна явно замыкать border, неявного closing edge нет;
- соседние совпадающие side planes объединяются с componentwise epsilon0.001;
- quad даёт one face+four side planes, activeCount5.

Whole `45EC70` на этих входах: behind sphere `(0,0,20,1)` исключается,
foreground`(0,0,5,1)` и side`(8,0,20,1)` доходят до реального support draw.
Hidden support slot становитсяnull, его mark64=Scene40−1.
Helper`4903E0` требует **строго** d>radius для каждой enabled plane, tangency
иNaN не считаются полностью скрытыми; cached count0→true.

Ещё подтверждена локальная failure policy: Scene **игнорирует**false от
`4702E0`; после actual geometry clear при сохранённой сфере/регистрации
предыдущие cached planes всё ещё используются. Это проверка контракта функции
на заданном состоянии, **не утверждение, что штатная игра рисует между clear
и повторным Init** или что найден новый пользовательский баг.

## Проверки и остаток

- `probe_pc_occlusion_runtime.py`: **84/84** (18 lifetime +8 weld +18 Scene +40 planes),
  четыре независимых bounded children, все normal-run allocations освобождены.
- `inspect_pc_occlusion_runtime.py`: **24/24** SHA/identity/table/path/CALL/gate anchors.
- `compare_pc_visibility_runtime.py`: **1483/1483** fields,
  sphere804/268cases +selection679/64cases; C++ visibility24 checks, CTest17.
- Portable change только fully-inside math helper плюс exact ABI. Полного
  portable `spOcclusionVolume` source пока нет.

Открыты full protected topology/Init и re-init lifetime, real decagon acceptance,
nonempty silhouette rebuild, paired-occluder geometry tests490480/490360,
runtime helper original names и source-to-Scene wiring. Следующая соседняя
зависимость — original Octree child queries, не случайный несвязанный класс.
