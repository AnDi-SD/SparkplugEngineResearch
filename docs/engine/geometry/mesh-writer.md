# PC DXMeshData: полная запись CPU-геометрии

## Исполненная цепочка

CPU IndexBuffer: constructor `45F7F0`, reader `45FB80`; VertexBuffer:
`45FE50`/`460300`. Настоящая factory MeshData `41A270` выделяет `0x58`;
`434E40(mesh,indices,vertices,0)` создаёт обе собственные глубокие копии и
вызывает расчёт bounds. Подготовленный SerializerManager предоставляет policy.
Настоящая factory DXMeshDataSerializer `4297C0` даёт `0x14`, secondary this `+0x10`.

Полный writer `429EA0` использует `473000`/`472710` и getter `419E10`:

| Policy | Порядок полей |
| --- | --- |
| 0, 2 | field 0: общий payload; field 1: PC payload; terminator |
| 1 | field 1: PC payload; terminator |

Оба поля резервируют UInt32 size code 7 (`E0`/`E1` + little-endian u32).
В общем поле `42B030` пишет полный serialized IndexBuffer, затем VertexBuffer.
PC helper `4298A0` разрешается через `13B2250` в `013BC5E0`: stack DXMeshData
`472560`/`472590` копирует буферы, пишет 17-byte planning header и ту же пару
полных serialized CPU buffers (`45FC90`, `460400`). `472E20` исправляет длины;
`472B00` завершает секцию нулём. Временные и входные owners освобождены реальными
destructors; итоговый live engine allocation set пуст.

Header: component flags, vertex count, expanded vertex byte size, index byte
size (четыре u32), Is32Bit (u8). Bit `0x20` добавляет `12*vertexCount` только
к planning size. Сами вершины в файле сохраняют четыре packed byte:
расширение в четыре float выполняет последующий reader/materializer.
Пример packed3: planning VB=84, serialized VB data=48, field1 size=95.
Пример triangle3: field1 size=83, вся секция policy1=89 bytes.

Caps не менялись: 100000 instructions/2s на original call, 30s на child,
64 KiB общей arena, 32 KiB на engine request. Native writer около 19–22 тысяч
инструкций, arena около 1–1.3 KiB; SDK/Direct3D для записи не нужен.
Это локальная секция MeshData, не whole FFPS save и не whole scene exporter.
Нет доказательства общего rollback, malformed-native safety или всех форматов.
