# PC: whole Skin scene и точность Node transform

CP107, 8 сентября 2026. Неизменённый `SFX/droid_trail.smo` целиком прочитан
original `422B50` и C++: совпали 13 объектов и 3569 байт состояния, слоёв,
матриц костей, buffers/declaration. Все имена, class IDs и ссылки по FAT ID
также точны. Это первый whole Skin SMO в текущем наборе сравнений.

Вход 3070 байт, platform1, SHA-256
`4781A76774FD2F079AF853B1ECC7235FB0B743FFFE071B34F9D8ACEED9C7AEA8`.
Native whole load: 423008 инструкций / 2,647 с, 113 assertions;
146 tracked allocations освобождены, COM refs/locks0. Arena77296 из128 КиБ,
один allocation до32 КиБ, COM buffer до1 КиБ. Сохраняются заранее объявленные
file limits 1 млн инструкций /8 с и внешний процессный предел30 с.

Граф включает восемь Node, RenderNode, Skin, DXMaterial, Fog и DXMesh.
Skin с weightCount0 содержит16 bindings: `[4,10,11,12,13,4,4,4,4,4,4,4,4,4,4,4]`.
Все16 inverse-bind matrices сравниваются побитно. Материал, туман и common
MeshData загружаются тем же whole loader, без подмены результатов reader.

## Исправления, найденные полным файлом

Первый завершённый native load выявил отличие только в world transform
RenderNode ID5. C++ округлял каждый промежуточный product/sum до float;
original `420350` и `420C00` удерживают x87 промежуточные значения до записи
компоненты. Vector helper суммирует индексы2,1,0; matrix helper имеет свой
порядок для каждой ячейки. Исходник теперь сохраняет этот порядок и использует
double промежуточные значения. После исправления whole capture совпал точно,
включая Z `-1.52587890625e-5` и слабые внедиагональные компоненты матрицы.

Отдельный пакет исполняет обе original функции без seams:196 случаев,
1176 точных float words,12642 инструкции,144 байта arena. В нём есть
катастрофическое сокращение, signed zero, identity и192 seeded finite cases.
Два C++ regression guards сохраняют остаток1 при сумме `1e8 + 1 - 1e8`.
Double не объявляется универсальной заменой 64-битной мантиссы x87:
экстремальные показатели, NaN и другие FP modes этим набором не доказаны.

Loaded Skin прежде удерживал кости через shared_ptr. В этом файле кость
является предком RenderNode, владеющего Skin: сильная обратная ссылка создавала
цикл владения. Original Skin хранит borrowed Node pointers. Загруженные bindings
теперь используют weak_ptr к явному владельцу в read context/scene graph;
временный lock удерживает кость на время доступа. Истёкший owner даёт отказ,
а не висячий указатель. Проверка actual source reader создаёт ancestor→RenderNode
→Skin→ancestor и подтверждает освобождение root и Skin после выхода владельцев.

Ручные host palettes и клонированные отдельные кости по-прежнему могут хранить
владельца явно. Это аналитическая политика памяти, не native intrusive ABI.
Стенд отдельного loaded-render теперь сам удерживает объекты read context.

## Воспроизведение и границы

```powershell
python research/probe_pc_scene_file_profile.py droid-trail --file-ids
python research/compare_pc_node_math_bits.py
```

[CP107 manifest](../../research/native-cycle-checkpoint-2026-09-08-cp107.json)
содержит результаты и fingerprints. CTest62/62; отдельные original/source
Skin read/write, clone и loaded-render проверяются существующими profiles.
RTTI/manager/COM остаются явными consumer fixtures. Полный CRT startup,
внешние зависимости, arbitrary cycles, runtime save и одновременный actor/render
frame открыты. Class scores за этот checkpoint не повышены.
