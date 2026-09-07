# PC общий MeshData writer

CP97, 7 сентября 2026: **30 exact captures / 210 native assertions** на тех же
10 CPU inputs и policies0/1/2, что и CP95. Оригинальные CPU readers и
MeshData `41A270`/`434E40` создают owning mesh. Factory serializer `42AEF0`
и whole secondary writer `42B170` исполняются без замены внутренних методов.

Policy0/2: BeginObject `472710`, WriteBegin(field0,sizecode7) `472D30`,
полный buffer helper `42B030`, WriteEnd `472E20`, terminator `472B00`.
Policy1 не вызывает buffer helper и сохраняет один нулевой byte. Никаких
PC-native полей у общего writer нет. Native writer policy1:1772 instructions;
policy0/2 около9800. Все CPU buffers/mesh/serializer/temporary list owners
освобождены; arena меньше1KiB на обычном triangle.

Source `spMeshDataSerializer` получил virtual WritePayload и общий
WritePayloadWithContext; manager policy проходит через существующий API.
Source field0 writer использует общие DataBlock и CPU buffer writers.
DX override CP95 остаётся отдельным собственным writer. Некорректный объект,
отсутствующие buffers в выбранном cross-пути и превышение32MiB — host guards.
В policy1 buffers не запрашиваются; результат — пустая корректная секция,
которую строгий source reader намеренно не принимает за initialized mesh.

Профиль `pc-mesh-base-writer`, comparer
`research/compare_pc_mesh_base_writer.py`, общий native probe
`research/probe_pc_mesh_writer.py` с `base=True`.
`SparkplugMeshReaderTests`: **298/298**; проверяются оба writer, policy omission
и повторное чтение заполненных полей production PC reader.

При финальном review добавлен host guard exact serializer type: неизвестный
derived writer (включая PS2) не наследует успешную PC omission-секцию. Проверка
отказывает до изменения output; новый PS2 behavior credit не начисляется.

Это уточняет исторический evidence-only writer dossier
`native-class-sp-mesh-data-serializer.md`. Whole native FFPS save, экспортные
dispatch registrations, полный FAT producer и rollback остаются открытыми.
