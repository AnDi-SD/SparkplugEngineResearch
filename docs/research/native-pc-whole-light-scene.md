# PC: целый общий SMO с runtime DXLight и DXMesh

CP105, 8 сентября 2026. Неизменённый `Characters/Bloom/bloom_projectile.smo`
прочитан whole original `422B50` и C++: шесть runtime объектов, их имена,
local/world state, поля света/Fog/Model, геометрия и связи совпали побайтно.
Вход770 байт, SHA-256 `BE5C62D8A9A00FCBB51E987C7C9FFBDC92433C0FA2BB81001E20A9A2419C928D`.

Файл имеет platform mask1 — общий формат, исполняемый на PC. Вместо предварительного
DX batch используется `spMeshDataSerializer` (`42AEF0/42AFD0/42B420`), который
создаёт runtime DXMesh. LightData header `4400B0` создаёт DXLight; выполняются
его reader `440640`, inherited Node reader, world update `4B58D0` и attachment.
Материал отсутствует; Ambient01 является вторым ребёнком Scene Root.

| Измерение | Результат |
|---|---:|
| Whole load | 204841 инструкций /1,514 с |
| Curated native assertions | 56 |
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

## Найденные зависимости и исправление

Первый C++ whole load остановился на FAT index: static library не удерживала
translation unit регистрации **wire LightData**, хотя её serializer создавал
runtime DXLight. Конструктор `spLightDataSerializer` теперь явно удерживает
`spLightData::StaticRTTI()`, по уже существующему host образцу MeshData.
Это исправление линковки реконструкции, а не найденный native startup body.
CTest проверяет самостоятельное чтение FAT entry LightData.

Первый native whole run прошёл mesh copy и остановился на cursor732:
resource manager запросил RTTI runtime DXMesh, отсутствовавший в исходном
наборе входных записей. Диагностическая трасса локализовала ID193B2671 и null
read47391B; она сохранена отдельно, stopped guest не возобновлялся. Новая
fixture содержит девять доказанных wire/runtime записей. Дерево проверяется
на сортировку, parent links, root color, отсутствие red/red и равную black
height. Engine lookup/classification исполняются без seams.

После этих исправлений fresh whole run завершился с точным сравнением и
cleanup. Прежний `logo_screen.smo` также заново прошёл: те же189653 инструкции,
707 state/layer/buffer bytes и все81 allocations освобождены. Его ранняя
[CP103 сводка](../../research/native-cycle-checkpoint-2026-09-08-cp103.json)
сохранена без изменения fingerprints.

## Воспроизведение

```powershell
python research/probe_pc_scene_file_profile.py bloom-projectile
python research/probe_pc_scene_file_profile.py logo
```

Профиль file:1 млн инструкций/8 с на вызов, process30 с, allocation32 КиБ и
arena128 КиБ. Source capture использует общий loader и существующие serializers.
Проверки C++ и Python, hashes и локализованный stop перечислены в
[CP105](../../research/native-cycle-checkpoint-2026-09-08-cp105.json).
Открыты textured/skinned/level whole files, полная регистрация startup,
runtime save graph и все failure/ownership варианты. Scores автоматически
не повышаются за ещё одну композицию доказанных операций.
