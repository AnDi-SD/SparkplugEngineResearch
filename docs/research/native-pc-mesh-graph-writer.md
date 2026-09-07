# PC MeshData: indexing и generic reference writer

CP98, 7 сентября 2026: **4 exact captures / 48 native assertions** — packed
mesh, base/DX serializer, policy0/1. После настоящих CPU readers и MeshData
initializer подготовленный manager содержит явную exporter registration
`MeshData33C34CF0 → выбранный serializer`, platformFF/save3, activePC/save2.

Whole `4672C0` индексирует mesh и исполняет native `5A7DB0`: true без
relationships. FAT получает один ID1; CPU index/vertex buffers остаются
локальным owning payload, не отдельными графовыми ресурсами.
Whole `467350` выполняет найденный writer, записывает header CPU MeshData
`33C34CF0/SBOO`, исправляет размер и actual FAT metadata
`[id,class,offset,size,written]`. Следующая ссылка — ID1,size0; null — u32zero.
Все bytes и metadata совпадают с compiled source generic protocol.

Первый подготовительный прогон выполнил writer, но teardown обнаружил четыре
неосвобождённых FAT container allocations. Исправлен порядок завершения
fixture: actual `466760` очищает FAT перед уничтожением ресурсов/manager.
После этого во всех четырёх случаях реальные destructors освобождают всех
tracked owners; нет ручного освобождения оставшихся engine blocks.
DX/packed/policy0: write24934 instructions,arena5600,output212 bytes.

В source оба MeshData serializer подключены к существующим
IndexReference/IndexResource/WriteReference через подтверждённый пустой
IndexRelationships. Host type guards сохраняют отказ неподтверждённым
derived serializers. `SparkplugMeshReaderTests`: **346/346**.
Профиль `pc-mesh-graph-writer`, comparer
`research/compare_pc_mesh_graph_writer.py`.

Регистрация exporter является явным input, а не восстановлением полного
startup registry. FAT directory/file-index producer и whole FFPS save не
заявлены; writer/reader file-byte acceptance отдельно проверена в CP96.
Оценки классов сохранены: это соединение уже исследованных producers.
