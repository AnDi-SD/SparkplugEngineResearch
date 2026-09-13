# PC `spOcclusionVolume`: геометрия и потребитель в Scene

## Что исполнено, а что ещё нет

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
| --- | --- |
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

## World, camera planes и Scene

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

Открыты full protected topology/Init и re-init lifetime, real decagon acceptance,
nonempty silhouette rebuild, paired-occluder geometry tests490480/490360,
runtime helper original names и source-to-Scene wiring. Следующая соседняя
зависимость — original Octree child queries, не случайный несвязанный класс.
