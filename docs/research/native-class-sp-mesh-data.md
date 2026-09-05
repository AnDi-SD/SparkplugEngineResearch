# Нативный класс `spMeshData`: точка продолжения

Дата разведки: 2026-09-04. Статус: native ownership/layout, initialization,
deep copy и clone boundary подтверждены на PC и PS2; переносимый класс и оба
обязательных buffer-типа реализованы и проверены.

## Идентичность

`spMeshData` имеет class ID `0x33C34CF0` и direct base `spMesh`
(`0x3F077B6C`) на обеих платформах.

| Факт | PC | PS2 |
|---|---:|---:|
| registration | `0x0075D428` | `0x004A81A0` |
| initializer | `0x006D1D00` | `0x00480E2C` |
| factory | `0x0041A270` (protected thunk) | `0x00134DB0` |
| registration getter | `0x00434E20` | `0x00135B20` |
| primary vtable | `0x006DE8FC` | `0x0048D9F0` |
| secondary vtable | `0x006DE8F4` | `0x0048DA14` |
| destructor | `0x00434EA0` | `0x0015D2B0` |
| RTTI clone | `0x0041AC00` | `0x00134CF0` |
| init/deep-copy inputs | `0x00434E40` | `0x0015D220` |
| deep-copy state | `0x00434F10` | `0x0015D1A0` |
| release buffers | destructor path | `0x0015D120` |
| размер | observed exact extent `0x58` | factory allocation `0x58` |

PC initializer передаёт literal ID/base, имя `0x006DBE68`, base registration
`0x0075E090` и factory. PS2 initializer делает то же с name `0x00445098`,
base registration `0x004A8CE0` и factory `0x00134DB0`.

## Layout и ownership

`spMeshData` добавляет к `spMesh` ровно два owning pointers:

| Offset | Тип/роль |
|---:|---|
| `+0x50` | `spIndexBuffer*` |
| `+0x54` | `spVertexBuffer*` |

Одинаковые offsets независимо видны в PC `0x00434E40/0x00434F10` и PS2
`0x0015D120/0x0015D1A0/0x0015D220`. PC вызывает deep-copy helpers
`0x0045FD90` и `0x00460240`; PS2 — уже сопоставленный deep-copy
`spIndexBuffer` `0x00159780` и vertex helper `0x0015CB30`.

Init создаёт обе копии, записывает их в `+0x50/+0x54`, затем вызывает
унаследованный `spMesh` bounds pass (`0x00424230` PC / `0x00159AE0` PS2).
Deep-copy state также переносит bounds min/max `+0x2C..+0x40`. Destruction
освобождает оба буфера и продолжает цепь `spMesh`.

PS2 constructor `0x0015D330` сам лишь вызывает `spMesh` и заменяет две vtable;
записей нуля в `+0x50/+0x54` в нём действительно нет. Дополнительная проверка
allocation chain `0x0010D850 -> 0x0010BFB0 -> 0x00406920 -> 0x00406C18`
показала обычное повторное использование free blocks без zero-fill. Значит,
factory `0x00134DB0` формально может вернуть объект с неопределёнными pointers
до init transaction. Это нативная шероховатость, а не скрытая гарантия.

Portable constructor намеренно ставит оба owner в `nullptr`. Это явно
документированная safety-divergence: воспроизводить неопределённые pointers и
риск destructor-а нельзя, а доказанные layout и последующая init-семантика от
этого не меняются.

RTTI clone не равен этому deep-copy: обе vtable оставляют в copy slot
унаследованный name-copy (`0x00413120` PC / `0x00105DC0` PS2). Clone wrappers
`0x0041AC00/0x00134CF0` создают объект и вызывают именно этот virtual slot.
Следовательно, как у `spIndexBuffer`, полноценное копирование геометрии живёт
в отдельном API (`0x00434F10/0x0015D1A0`), а RTTI clone является blank runtime
clone. Это различие должно сохраниться в будущей реализации.

## Реализованный срез

`spVertexBuffer` теперь восстановлен отдельно до exact `0x5C`, поэтому
`spMeshData` хранит конкретные owning `spIndexBuffer`/`spVertexBuffer`, а не
`void*` и не заранее декодированный geometry vector.

`InitializeForAnalysis` глубоко копирует оба входа и читает только доказанный
XYZ-prefix каждой вершины для унаследованного bounds pass. Отдельный
`CopyMeshDataForAnalysis` копирует buffers и шесть min/max float, но, как
native `0x0015D1A0`, не переносит bounds-valid byte. Обычный `Clone()` копирует
только inherited resource name и оставляет оба buffers пустыми.

Тесты различают все три операции и проверяют ownership, bounds и class/layout
identity. `spModel` после этого использует concrete `shared_ptr<spMeshData>`, а
не временный широкий `spBaseObject` seam.

Открыты original header/TU/API, роль secondary vtable/interface, причины
неинициализированных native owners, trailing поля `spMesh`, platform-specific
mesh subclasses и точные failure/rollback semantics. SMO container grammar
остаётся в отдельной карточке [`smo-class-sp-mesh-data.md`](smo-class-sp-mesh-data.md)
и не подменяет native object API.
