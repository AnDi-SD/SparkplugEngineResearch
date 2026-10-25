# PC `spOcclusionVolume`: геометрия и потребитель в Scene

## Реализованная граница

[Общий класс](../../../Sparkplug/Code/Sparkplug/spOcclusionVolume.h),
[fresh Init и world update](../../../Sparkplug/Code/Sparkplug/spOcclusionVolumeRuntime.cpp)
и [камера](../../../Sparkplug/Code/Sparkplug/spOcclusionVolumeCamera.cpp)
содержат тела алгоритмов. Полный исходный `470FE0` и последующие
`46DD90/46FFE0/4702E0` возвращаются на конечных входах с настоящими CPU
буферами и геометрией. Кеш не подставляется вместо построения силуэта.
Сравнение текущего C++ проверяет значения и порядок полученных плоскостей,
точек силуэта, классификаций граней и отметок обхода.

Это граница fresh-объекта с конечной геометрией, а не полнота класса или
Scene. Повторный Init и полная регистрация в Scene/partition остаются
отдельными контрактами. Внешняя CRT-сортировка задаётся явно, а ограничения
host на размеры, конечные числа и корректность ссылок не выдаются за
валидацию оригинала.

## Identity, layout и lifetime

Original ID `43D24430`, direct `spNode695C0F65`, registration `760640`,
factory `470A70`, exact allocation **1B8**. Primary `6E8DE0`, ровно14 слотов:

```text
46FFC0 420B40 470AD0 421F80 46FD70 408350 408370
46D7E0 46D520 4212F0 420610 421330 46DD90 421640
```

За таблицей уже строковые данные, не дополнительные virtual methods.
Protected ctor `46FE60`/slot`13B324C` и dtor `46FC50`/slot`13B1C28`
исполнены; deleting wrapper `46FFC0`. Восстановленный exact ABI хранится в
[SparkplugAbi.h](../../../Sparkplug/Analysis/PC/SparkplugAbi.h) отдельно от
общей C++ модели; адреса и размещение её контейнеров не являются PC ABI.

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
outgoing edges, walk stamp, border byte. Сначала строится и преобразуется
граф топологии, затем `46FFE0` записывает классификации граней и отметки
силуэта. Неинициализированное исходное слово camera-side до первого такого
вызова в общей модели обозначается как неизвестное.

Clone `470AD0` создаёт новый1B8, register`412F70`, вызывает inherited
`Node421F80` через v0C. На подготовленном исходном объекте подтверждено:
**собственные буферы/силуэт/init state не копируются**. Имена и дети здесь
пустые; special nonempty-name alias lifetime этим тестом не покрывается.

## CPU geometry preparation

Readable Init`470FE0` сначала вызывает clear`46F880`:
direct deletes IB/VB/worldVB, обнуляет три указателя, удаляет edge objects,
сбрасывает initialized. При этом clear **не стирает cached plane set**.

На fresh-объекте Init копирует IB с приведением индексов к UInt16 и создаёт
position-only local VB. Weld remap и compaction используют внешний sort
dispatch; затем отдельный world VB даёт адреса точек для граней и рёбер.
Драйвер строит/связывает граф, удаляет coplanar edges и обрабатывает
планарную обратную сторону. Local sphere вычисляется по исходному VB,
включая вершины, которые затем отбросит compaction. Local mins/maxs начинают
с нулей, поэтому могут включать origin вне диапазона самих вершин.
Initialized и camera-dirty становятся true после успешного конца.

Общая реализация принимает fresh-объект, не более4096 вершин и1024
треугольников. Оригинальный повторный Init имеет наблюдаемые stale-pointer
и repeated-free пути. У одного треугольника исходное достраивание обратной
стороны может оставить ссылки рёбер на прежнее размещение граней. Эти пути
не заменяются исправленной топологией: host явно отказывает до полного
построения; отдельный triangle producer остаётся доступным.

## World, camera planes и Scene

Унаследованный `421640(enabled,recursive)` не unregister-ит occluder;
actual Scene продолжает учитывать отключённый узел. Reverse drain`46DCF0(true)`
уведомляет root v38, опустошает логический список, сохраняет capacity;
false освобождает его без уведомления. Original teardown освобождает все buffers.

`46FFE0` до plane builder: initializedfalse→false без чтения геометрии.
Иначе при !dirty и `abs(currentCamera[i]−cachedCamera[i])<=0.001f` для всех
трёх компонент использует кеш. При таком попадании cached position и walk
stamp не меняются. Unordered comparison не даёт попадания. Dirty заставляет
перестроение даже при том же положении камеры.

Rebuild публикует world camera и очищает dirty, вычисляет local camera через
`420710`, очищает border и camera-face-plane vectors. Обратный transform
вычитает cached world position и использует cached world orientation/scale.
Reciprocal X остаётся extended; reciprocal Y/Z и три результата rotation
промежуточно сохраняются во float перед масштабированием. Общая конечная
арифметика находится в [spOcclusionCameraMath.h](../../../Sparkplug/Analysis/PC/spOcclusionCameraMath.h).
Нулевая scale и переполнение промежуточного float — явная incomplete область
host, без утверждения о native Inf/NaN поведении.

Классификация local face distance: `<-0.001f`→1, `>+0.001f`→0, иначе2.
Для каждой грани класса0 её world plane строится по сохранённым position
references в обратном порядке `(2,1,0)`. Силуэт начинается с первого ребра,
у которого own face имеет класс0, а opposite отсутствует или имеет другой
класс. Walk stamp увеличивается и при отсутствии подходящего ребра.
Обход добавляет start первой дуги, затем end каждой посещённой дуги,
помечает дугу текущим stamp и выбирает первое подходящее outgoing edge.
Числовой border byte не заменяет эту проверку. Штампованные дуги завершают
цикл; нативное UInt32 переполнение stamp сохраняется.

`4702E0` вызывает это построение и при false сохраняет прежние planes.
При true очищает plane-set logical size и active count:

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

Открыты re-init lifetime, acceptance произвольной геометрии, paired-occluder
geometry tests490480/490360, runtime helper original names и полная
source-to-Scene wiring. Квалификация конечных quad, извлечённой геометрии,
отдельных signed-scale/rotation transforms и inverse inputs не закрывает
произвольные повороты, масштабы, геометрию или весь Scene renderer.
