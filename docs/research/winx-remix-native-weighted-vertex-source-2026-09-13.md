# Native weighted vertex inputs: захват до D3D

13 сентября 2026. Завершён захват происхождения weighted/packed vertex bytes.
Weighted geometry **не включена** в Remix Resolve или API draw. Этот checkpoint
не подтверждает правильность деформации установленным renderer.

Общий [spPCDXVertexBytes.h](../../Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h)
содержит арифметику, извлечённую из существующего
`spDXMesh::BuildVertexBytesForAnalysis`. Сам `spDXMesh` и собственный
[native mesh adapter](../../research/rtx-remix/winx_native_mesh_source.h)
вызывают один helper. Для восстановленного класса используется `vector<byte>`,
для снимка адаптера — `vector<uint8_t>`; алгоритм общий.

Это рефакторинг уже восстановленного поведения PC `429A40 → 4AA000`, а не новое
восстановление по ожидаемому результату Remix. Полная цепочка подготовки
`4A99A0/4AA000` и materialization исследованы ранее; см.
[native DX materialization](native-pc-dx-materialization.md) и
[исходный native mesh checkpoint](winx-remix-native-mesh-source-2026-09-13.md).

## Подтверждённое преобразование

- Без `componentFlags & 0x20` копируются ровно `vertexCount * sourceStride`
  исходных байтов. Веса, alpha/COLOR0, normals и UV не пересчитываются.
- При `0x20` offset packed поля берётся из **`componentOffsets[6] * 4`**.
  Четыре uint8 превращаются в четыре float32 со значениями `0..255` без
  нормализации. До поля и после него копируются исходные байты; stride растёт
  на 12. Преобразование применяется отдельно к каждой вершине.
- Остальные native states по-прежнему производит оригинальный initializer.
  Сохранена и известная особенность packed combiner path: native stored
  `vertexByteSize` может отличаться от числа фактически скопированных байтов
  из-за дополнительного прибавления `12 * vertexCount`. Адаптер не исправляет
  этот field и проверяет фактический диапазон по mapped stride и VB capacity.
- Общий wrapper по-прежнему проверяет `initialized`. Общий helper проверяет
  available bytes, packed offset и uint32 destination size. Дополнительные
  null/overflow guards — наши границы безопасного host-вызова. Native stride
  в `spVertexBufferLayout` имеет размер uint16, поэтому новый uint32 guard
  сложения stride+12 не меняет допустимые native inputs.

Имена нового helper и его входных параметров аналитические. Он не вычисляет
skinning, matrix palette, inverse bind pose, нормализацию весов или lighting.

## Граница захвата и владения

`MeshInitialize` читает authored `spIndexBuffer`/`spVertexBuffer` до единственного
исходного вызова. Сохраняется старый uint16 index cohort. Захваченные vertex
bytes преобразуются общим helper до D3D; в `Bytes` хранится результат этого
подтверждённого отображения, а не GPU readback. Исходный CPU buffer не меняется.

Оригинальный initializer получает те же `this`, index/vertex objects и keepCPU,
выполняется ровно один раз и возвращает неизменённый полный EAX. AL=0 запрещает
публикацию. Затем проверяются exact native mesh/VB/IB identities, captured flags,
vertex count, **mapped** stride, index type, ненулевые разные COM addresses и
полное попадание интервалов в фактические native buffer capacities.

Existing capture caps сохранены: 8 MiB на буфер, 64 MiB retained storage,
4096 records и не более 4096 известных интервалов на record. Новый mapped-size
guard срабатывает до выделения преобразованного массива. Combined ranges
остаются частичными; неизвестный промежуток не становится пригодным для чтения.

Перед изменением обоих record их поколения становятся нулевыми. Свежий общий
generation публикуется после обеих копий. При исключении во время публикации
оба record удаляются; нулевой generation не допускается в resource resolution.
Это поколение CPU capture, **не** поколение Create, COM lifetime или scene.
Поздние consumers всё ещё обязаны проверить existing `TransportWitness`.
Writable Lock, final Release, recreation и Reset сохраняют прежнюю инвалидацию.

Оба geometry Resolve entry сохраняют отказ по weighted/packed flags. Добавлены
include `winx_native_skin_source.h` после owner header и его `Capture` в valid
Submit scope; этот отдельный palette observer не получает права подавлять draw.

## Проверки

| Проверка | Результат | Граница |
|---|---:|---|
| Новая x86 CPU capture fixture | **535 checks, 47 original calls** | Owned ABI records, mocked original; без COM/GPU/игрового кода |
| Общий `SparkplugMeshReaderTests` | **350/350** | Восстановленные классы и serializers |
| Общий `SparkplugVertexDeclarationTests` | **338/338** | Общая native declaration/materialization логика |
| `pc-mesh-readers-output` | **6/6** | Original instructions против пересобранного общего source; по 10 exact output checks |

[CPU fixture](../../research/rtx-remix/test_native_vertex_capture.cpp) проверяет
packed/unpacked × weighted/rigid, literal float4 expansion, сохранение исходных
байтов, вызов original после capture, disjoint/merged combined ranges,
fresh generation, смену native identity, выход за диапазоны, unreadable memory,
flags/stride/count mismatch, AL=0, disabled/foreign-thread path и Forget/release.
Она не выдаёт mocked original за выполнение оригинальных инструкций.

Фактический x86 результат:
`local-data/rtx-remix/native-vertex-capture-tests/weighted-provenance-v1`.
Общие сборка/результаты и source snapshot:
`local-data/rtx-remix/native-vertex-capture-common-v2`.
Финальный original/source report:
`local-data/results/bounded-native-runs/20260913T052336990539Z-pc-mesh-readers-output.json`.

Initial Windows macro incompatibility (`max`) обнаружилась в отдельной сборке
palette observer и исправлена скобками без изменения арифметики; её failed log
сохранён владельцем observer. Первые common-v1 проверки сохранены; финальный
common-v2 пересобран после этого исправления. Затем существующие изменённые
файлы возвращены из CRLF в исходный LF, чтобы не раздувать diff. Поведение
повторно не прогонялось: manifest сохраняет raw hashes compiled snapshots и
текущих файлов, а также доказывает равенство их LF-normalized текста.

Установка DLL, открытый игровой observer run и последующие GPU skin comparisons
не входят в этот checkpoint. Полные source/evidence hashes находятся в
[manifest](../../research/winx-remix-native-weighted-vertex-source-2026-09-13.json).
