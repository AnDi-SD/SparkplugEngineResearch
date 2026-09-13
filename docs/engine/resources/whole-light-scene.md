# PC: целый общий SMO с runtime DXLight и DXMesh

Файл имеет platform mask1 — общий формат, исполняемый на PC. Вместо предварительного
DX batch используется `spMeshDataSerializer` (`42AEF0/42AFD0/42B420`), который
создаёт runtime DXMesh. LightData header `4400B0` создаёт DXLight; выполняются
его reader `440640`, inherited Node reader, world update `4B58D0` и attachment.
Материал отсутствует; Ambient01 является вторым ребёнком Scene Root.

| Измерение | Результат |
| --- | ---: |
| Runtime объекты | 6 |
| State bytes | 476 |
| Index / vertex / declaration | 68 /312 /32 байта |
| Общая bump arena | 71968 из131072 байт |
| Освобождённые native allocations | Все78 |

В сравнении исключены opaque light wordDC и неинициализированные cache words
DXLight. Порядок и значения сериализуемых полей проверяются вместе с Node
состоянием. COM buffers после teardown имеют refs/locks0; общего combiner в
этом пути нет. Физический D3D device, полный CRT startup и renderer frame
данный опыт не выполняет.
