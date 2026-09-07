# PC: экспорт MeshData из исходников принимает original reader

CP96, 7 сентября 2026. **10 exact comparisons / 290 native assertions**:
triangle, packed component20, uint32 indices, component840, nonzero flags,
каждый в policy0 и policy1. Протокол:

1. Original CPU readers → actual MeshData initializer → whole original
   DXMeshData writer (CP95).
2. Те же два входных CPU buffer stream переданы compiled source writer.
   Все output bytes сравниваются с original.
3. Именно **source output** передан свежему bounded original PC reader:
   `4297C0` factory → `42AFD0` header/factory → whole `429BC0` → `429A40`
   → CPU readers `45FB80`/`460300` → `4AA000`/`4AE0E0` → COM copy/declaration.
4. Compiled source читает собственный output. Сравниваются FVF, stride,
   vertex/index byte size, weight count и **все vertex/index buffer bytes**.
   Original полностью освобождает mesh, buffer wrappers, declaration/map,
   serializer/managers и external COM handles.

Между записью и original read стоит явно объявленная граница file bytes:
это свежие emulator instances, не доказательство общей startup/lifetime сцены.
Перед локальной секцией fixture добавляет стандартный 8-byte object header
`33C34CF0/SBOO`; FAT и whole FFPS envelope здесь не выдаются за native export.
Direct3D — ограниченный COM fixture без настоящего GPU.

Read около 30–31 тысяч инструкций; packed-case arena63760, uint32-case63824,
flags-case63696. Общая arena64KiB и per-call100000/2s, child30s сохранены.
Сосуществующие cross/native поля приводят к одной materialization.

Профиль `pc-mesh-writer-roundtrip`, script
`research/compare_pc_mesh_writer_roundtrip.py`; source CLI
`SparkplugMeshReaderTests --write-roundtrip MODE POLICY`.
Полная сборка и **CTest61/61, 53.58s** после CP95–96 успешны.
Оценки классов удержаны: новый срез связывает ранее проверенные writer/readers.

Full FFPS save/FAT producer, arbitrary meshes/graphs, exception rollback,
device loss и настоящий display остаются отдельными открытыми задачами.
