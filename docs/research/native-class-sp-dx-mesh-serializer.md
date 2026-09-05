# `spDXMeshSerializer`: PC wire-связь runtime-меша с общими DX-буферами

Статус: RTTI, обе vtable, lifecycle, target, relationship class, полный порядок
чтения/записи и relationship-index pass подтверждены по pristine PC executable.
Класс отсутствует в PS2 executable; там работает отдельный backend `spPS2Mesh`.

## Идентичность

- class ID `0xE712BCAD`;
- direct base `spSerializer` (`0x42429877`);
- target `spDXMesh` (`0x193B2671`);
- reader запрашивает load relationship `spDXSharedMeshData` (`0x293A2681`);
- writer передаёт source relationship `spDXCombinedVB` (`0x4B18E622`);
- registration `0x00763BA0`, initializer `0x006D51C0`;
- primary vtable `0x006F0034`, serializer-interface vtable `0x006F0028`;
- exact implementation path `Z:\Sparkplug\Code\SparkplugDX\spDXMesh.cpp`.

Secondary interface начинается по `+0x10`, следовательно доказан полный prefix
`0x14`. Прямой `sizeof` не заявлен: factory entry `0x004B16A0` защищён.

## Wire-порядок

Reader `0x004B1780` последовательно получает:

| Порядок | Значение | Размер |
|---:|---|---:|
| 1 | `eIndexType` | `u8` |
| 2 | vertex-component flags | `u32` |
| 3 | D3D9 FVF | `u32` |
| 4 | relationship; read target `spDXSharedMeshData` | variable |
| 5 | index begin | `u32` |
| 6 | vertex begin | `u32` |
| 7 | index count | `u32` |
| 8 | vertex count | `u32` |
| 9 | vertex struct size/stride | `u32` |
| 10 | bounding-sphere center | `Vector3` |
| 11 | bounding-sphere radius | `float` |

После успешного чтения он строит `BoundingSphere` и вызывает shared-buffer
initializer `spDXMesh` со всеми десятью значениями. Любой неуспешный read или
`Init` возвращает false; диагностические строки сохраняют исходные имена всех
локальных значений.

Writer `0x004B1B20` выдаёт тот же порядок. Первые три и bounding sphere он берёт
непосредственно из `spDXMesh`. Но relationship и пять параметров диапазона
получаются через renderer-owned объект:

- `0x004BEDF0(spDXSceneGraphOptimizer, mesh)` возвращает `spDXCombinedVB`,
  локально названный в диагностике `pVertexBuffer`;
- common `SerializeRelationship` записывает его в object graph;
- `0x004C07E0(pVertexBuffer, mesh, ...)` возвращает index/vertex begin,
  index/vertex count и vertex struct size.

Relationship-index method `0x004B1DB0` повторяет первый renderer lookup и
передаёт результат common `IndexRelationship`. Это доказывает, что корректный
export нельзя строить простым дампом указателей или локальных GPU-wrapper-ов:
сначала должен существовать renderer/shared-buffer materializer.

## Portable boundary

`spDXMeshSerializer` реализован в том же reconstructed translation unit. API
принимает отдельный codec для variable-size relationship: его байтовая грамматика
принадлежит общему graph serializer-у и не подменяется искусственным форматом.
Scalar payload при известном relationship полностью читается и записывается;
reader материализует валидированный диапазон через `spDXMesh`.

`BuildPayloadForAnalysis` требует явно переданный renderer-selected
`spDXCombinedVB`. Opaque codec принимает `spBaseObject`, поскольку при записи
это combined VB, а после native ID-remap при чтении — `spDXSharedMeshData`.
После разбора владельца range builder больше не выводит пять слов из полей
runtime-меша. Он требует, чтобы supplied combined VB содержал проверенную
mesh→range ассоциацию, и буквально переносит из неё `indexBegin`,
`vertexBegin`, оба count и stride — как native `0x004C07E0`. Отсутствующая или
выходящая за raw payload ассоциация блокирует writer до записи байтов. Это
сохраняет границу всё ещё защищённых тел `0x004BEDF0/0x004C07E0`, не теряя уже
доказанный внешний контракт.

## Проверка и открытое

`research/inspect_dx_mesh_serializer.py` выполняет 33 read-only проверки:
контрольные SHA-256, отсутствие класса на PS2, hashes десяти bodies, registration,
две vtable, relationship ID, offsets полей и renderer calls. CTest проверяет
точный unaligned scalar order вокруг тестового relationship token, полный
round-trip в `spDXMesh`, отказ на обрезанном хвосте, ABI и blank clone.

Открыты direct `sizeof`, original header/interface names, внутренняя common
relationship encoding, original имя локального `pVertexBuffer`, защищённая
реализация optimizer lookup/grouping, ownership/fixup повторных ссылок и точный
rollback после частично прочитанного потока. Внешняя семантика обоих helper-ов
и расположение диапазона уже закрыты.
