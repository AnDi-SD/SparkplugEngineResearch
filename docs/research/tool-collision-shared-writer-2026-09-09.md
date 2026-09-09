# Общая запись MeshBV — 9 сентября 2026

Блок16 цикла до07:30; PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

`SmoCollisionBranchAppender` больше не кодирует MeshBV geometry самостоятельно.
Общий `SmoMeshBoundingVolumeWriter` передаёт positions/triangle indices через
`spv_mesh_bv_write_triangles` настоящим CPU buffers, owning CollisionMesh/MeshBV
и `spMeshBVSerializer`. Header, field0, IB/VB grammar и terminator исходные.
Face records этот путь не создаёт. Прежние CollisionInfo/registration/FAT edits
остаются отдельными операциями; всё collision editing завершённым не объявляется.

Host preparation buffers общая с render-mesh writer блока15. Оба пути используют
исходную component table. Восстановленная ownership assignment вынесена в
`spMeshBVSerializer::CreateGeometryForAnalysis`: это portable facade над прямыми
записями PC438604/438607 в свежий CollisionMesh. Original reader и tools используют
одну assignment. Метод не выдаётся за дополнительный original virtual method.
Fresh owners и порядок host allocations не реконструируют адреса исходного heap.
Bounds/ownership portion `SetDataAndBoundsForAnalysis` прежняя; OPCODE queries
этим API не предлагаются. DirectX/device вызовов нет.

Host limits: nonempty UInt16 triangle lists, vertices≤65536, indices≤3000000,
буферы≤16MiB. Более строгий существующий level-appender требует минимум4 vertices
и12 indices; его политику этот блок не меняет. Вначале трёхвершинный тестовый
вход закономерно отклонён, затем выбран tetrahedron. tile_bad/PC menu не имеют
подходящего зарегистрированного шаблона для этой операции: baseline отказал до
изменения writer. Для проверки выбраны Alfea01/02/03, поддерживающие appender.
Эти исходные ограничения не обходились и не маскировались.

## Проверки

`research/validate_tools_collision_writer.py` заново исполнил оригинальные
MeshBV factory, reader438490, bounds/tree preparation, writer438680 и owning
teardown для triangle и tetrahedron. Изменяется только явно заданный fixture
input; функции guest не подменяются. Полные tool objects равны original fields
с общим object header. Проверены три host refusals. Все original owners освобождены.

C++ MeshBV83; дополнительный повтор девяти original render-mesh writer cases
проверяет вынесенную общую input preparation. Importer compiled0 warnings/errors.
На Alfea01/02/03 добавлены три полные collision branches; контейнеры и MeshBV leaves
побайтно совпали с прежним writer, sources остались неизменными. Интеграционный
appender сам проверяет group2, geometry, triangle order и world placement.

Reports: `local-data/results/tools-core-cycle-20260909-0730/collision-writer/`;
hashes и точные границы — в snapshot. Полный корпус и релиз не запускались.
