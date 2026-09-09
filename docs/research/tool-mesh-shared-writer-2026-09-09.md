# Общая запись MeshData для tools — 9 сентября 2026

Блок15 цикла до07:30. PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

`spv_mesh_write_triangles` принимает typed host vertices и UInt32 indices,
создаёт настоящие `spVertexBuffer`/`spIndexBuffer`, затем owning `spMeshData`.
Stride/attribute offsets даёт единственная исходная component table; назначение
DTO полей в буфер — host adapter без normalization/inferred weights. Поля,
planning header, индексы, VB header и terminator записывают исходные
`spMeshDataSerializer` (portable policy0) или `spDXMeshDataSerializer` (PC policy1).
Object header также общий. Исходники движка не изменялись.

ABI2 дополнен одной функцией; результат — существующий SerializedBytes owner.
Host guard:1..65536 vertices,1..3000000 indices, полные UInt16 triangle lists,
суммарные VB/IB bytes≤16MiB, finite attributes, только представленные DTO
components и отсутствующие либо четыре веса. Эти ограничения принадлежат
инструменту, а не объявляются условиями исходной игры. Границы проверяются
до крупных allocations. DirectX/device/combined-mesh path не вызывается.

## Подключённые потребители

Общий C# `SmoMeshDataWriter` в Editing преобразует существующий SmoMesh DTO
в native input и копирует результат. Удалены три собственных сериализатора:

- `SmoMeshResourceReplacer`: замена физического MeshData, включая общий путь
  импортёра и level editor; оба E0/E1 выходных варианта.
- `SmoSkinnedBranchSplitBuilder`: запись созданных opaque/alpha skinned chunks.
  Retarget/распределение весов импортированного донора остаётся host conversion;
  функция возвращает typed vertex вместо самодельного игрового byte record.
- `SmoSkinnedVisualGraphRebuilder`: одновершинный skeleton carrier с вырожденным
  треугольником также записывается тем же writer.

Убрано около330 строк старого кода, добавлены небольшие DTO вызовы. FAT repack,
Skin/Material graph editing и collision MeshBV writer ещё требуют отдельной
работы. Этот блок не объявляет всё ядро импортёра завершённым.

## Проверка

`research/validate_tools_mesh_writer.py` заново исполняет исходные CPU IB/VB
readers, MeshData initialization434E40, base writer42B170/42B030 и DX writer
429EA0/4298A0. Все owning objects освобождаются исходным fixture. Его вход
расширен явными bounded bytes, исполнение guest-кода не подменено.

Девять original/bridge field streams совпали побайтно: portable/PC для triangle,
normal+UV, packed bones, full0x197E (четыре веса, bones, normal длины2, color,
два UV), плюс PC carrier с одной вершиной. Проверены пять host refusals.
PC временный DXMeshData writer сохраняет packed CPU bytes; planning size
рассчитан исходником. Известная отдельная неточность cached byte size после
MeshCombiner из блока7 здесь не затрагивается.

C++ MeshReader346. Девять замен на пяти файлах (gem, bloom_jeans, Icy,
PC menu, Alfea02) —91 check, шесть реальных layouts. Все полные outputs
побайтно равны контрольным, исходные файлы сохранены. Clean skinned graph
на Icy/Bloom —26 checks; два полных outputs тоже побайтно равны baseline.
Managed build импортёра прошёл без warnings/errors. Level Core проверяется
как потребитель того же writer; результат записан в build log/snapshot.

Reports: `local-data/results/tools-core-cycle-20260909-0730/mesh-writer/`.
В snapshot включены working-file и Git-blob hashes, поскольку C# text files
хранятся с CRLF в рабочем дереве и LF в Git. Релиз и полный корпус не запускались.
