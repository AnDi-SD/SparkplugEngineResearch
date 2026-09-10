# Общий Occlusion shape producer и driver

В [spOcclusionVolume](../../Sparkplug/Code/Sparkplug/spOcclusionVolume.h)
перенесены PC face helper4708C0, triangle producer470B20 и shape driver470E30.
Это следующий срез после [исторической producer boundary](tool-occlusion-init-producer-boundary-2026-09-10.md):
общий код теперь воспроизводит построение topology из явно подготовленных
XYZ/UInt16 triangles. Runtime Init, serializer и whole-graph регистрация
OcclusionVolume этим изменением не добавлены.

## Оригинальный источник и перенос

Один fresh `character` guest исполнил470E30 на неизменённых IB/VB полях
resource8, physical index7 `oclusion_wall02`, из PC `race_02.smo`.
Actual readers45FB80/460300 и copy460240 подготовили данные; привязка трёх
буферов была объявленным host input. Driver вернул AL1 за3958778 instructions,
9.8830789s. Все tracked allocations освобождены; arena использовала53888
из65536 bytes. Профиль выбран до первого вызова:4M/16s per call,30s child,
один worker. Peak host memory не измерялся.

После естественного VM byte-loop exit88A34B/cursorB20073 сохранены1123 bytes
13B4F48..13B53AA, SHA256
`E2B9EE0751BD98FA4E63720B471AD9598860D171DAB896B3B0262CB1B56E73B7`.
Exit наступил на3932573 instructions; остановка/возобновление и подмена
producer не применялись. Интервал начинается после FS-prefix64 при13B4F47;
CFG учитывает границы инструкций и явные return targets, а не padding.

| PC anchor | Сохранённое поведение |
|---|---|
| 4708F1 /470924..470A30 | Общий plane471420; первое совпадение всех4 scalars с epsilon0.001 сохраняет старые position pointers, иначе append face |
| 13B4FA8..13B534E | UInt16 triples в исходном порядке; три edges `(a,b),(b,c),(c,a)`, own face, null opposite, empty outgoing, исходный walkStamp, border1 |
| 13D04DC..13D0592 | Producer, merge, opposite links, coplanar removal, planarity и первый outgoing вызов проверяют AL; ранние мутации сохраняются при отказе |
| 13D062C..13D06DC | Planar reverse side добавляется в обратном порядке исходных edges; plane пересчитывается по обратному порядку retained face positions |
| 13D06E3..13D0713 | AL второго outgoing вызова игнорируется; старые outgoing lists и borderCount сохраняются |

Расчёт плоскости непосредственно использует существующий
`spZonePortal::PlaneFromFirstThreeForAnalysis`; новой копии математики нет.
Camera-side word face+1C исходный helper не инициализирует: общий
`cameraSideKnown=false` отделяет unknown от нулевого host storage.
Protected856CF0 совместно меняет EAX и сохранённый указатель: отдельный root
reader observer подтвердил `31010FEE + FFFFBF46 = 3100CF34` mod32, затем
cursor+6 и исходные triples. EAX сам по себе не является offset4.

`SetShapeBuffersForAnalysis` — отдельная host подготовка с проверками finite
XYZ, индексов,4096 points/1024 triangles. Она не читает buffer и не выполняет
game Init. Faces резервируются по triangleCount, как в13D0486; реализация
allocator не объявлена восстановленной. **Полный host shape требует минимум
два треугольника**. Пустой input исключается перед исходным unchecked
first-edge access; standalone zero/single-triangle producer остаётся доступен.
Повторный Build на мутированном состоянии этой проверкой не подтверждён.

Причина single-triangle границы установлена отдельным original
`fresh-init-batch-run1`: свежий positive-u32 объект вернул AL1/initialized1,
но после reverse append первые три edge.own не входят в текущий faces vector.
Capture `own_face=null` означает не найденный адрес, а не доказанный NULL.
При reserve1 рост до двух faces переносит storage без перепривязки прежних
указателей. Первоначальное host reserve `2*triangleCount` скрывало это отличие;
его убрали. Host guard возвращает unsupported до мутаций и **не приписывает
игре false**. Для принятого успешного planar input с >=2 triangles хватает
исходного reserve; single-triangle producer не добавляет reverse side.

## Проверка и оставшаяся граница

[Native tests](../../Sparkplug/Tests/spOcclusionTopologyTests.cpp) сравнили
actual checkpoint1face/6edges и итог2faces/8edges, обе плоскости побайтно,
retained position pointers, edge/outgoing order, borderCount4 и planar1.
Back plane пересчитана: её Z не равен точному отрицанию front Z.
Отдельно проверены partial link-failure композиции и host input guards,
включая шесть проверок новой single-triangle границы и сохранённого producer.

Финальная центральная проверка: **OcclusionTopology149/149 (0.68s),
FullLoader213/213 (1.14s)**. В том же запуске прошли MaterialSerialization653/653
(0.98s) и MaterialColor235/235 (0.91s): CTest4/4, total5.37s. Проверенная DLL:
`939B42687CE8806F910334D17B19AB3917769F08A867D040F6041B53A3456D97`.

История до нового guard:143/143 (1.04s), FullLoader213/213 (5.17s), CTest2/2,
total7.53s; этот прогон не проверял финальное ограничение. Первая сборка
остановилась на test-only `std::bit_cast`, недоступном в C++17; root заменил
его на `memcpy`. Исходный error log и последующий успешный log сохранены.

Standalone driver оставляет initialized0. Отдельные original fresh Init/reader
root уже исполнил, но общий full Init/serializer пока отсутствует. Общего
geometry optimizer460D90/460C40 и compactor4609A0 нет; точный CRT tie order
для duplicates неизвестен, а raw IB/VB зависит от representative. Уникальные
позиции выбранного входа устраняют ties только для него и не разрешают
подменять weld пропуском. [Отдельный re-init/batch разбор](tool-occlusion-reinit-and-batch-2026-09-10.md)
фиксирует границы повторного Init и single-triangle storage; shape tests не
подтверждают re-init или cleanup retained state и не доказывают ошибку игры.
PS2 static служил ориентиром; PC перенос опирается на PC bytes
и PC результаты, без PS2 runtime/numeric equivalence claim.

Локальные captures и logs находятся в
`local-data/results/tools-core-cycle-20260910-0730/occlusion-init-next/`;
финальные build/CTest logs — в `material-preview/fallback-material/`
того же cycle (`shared-color-shape-native-build.log`, `shared-color-shape-ctest.log`).
исходные игровые файлы и извлечённые bytes не входят в Git.
[Manifest](../../research/tools-core-occlusion-shape-block-2026-09-10.json)
фиксирует source/proof/test fingerprints. Старые manifests не переписаны.
