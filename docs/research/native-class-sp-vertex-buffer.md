# `spVertexBuffer`: component layout and owned CPU vertex bytes

Статус: identity/direct base, exact `0x5C` layout, component-mask expansion,
allocation/release, three-word stream grammar, blank RTTI clone and separate
deep-copy helper подтверждены на PC и PS2. Имена component enum и большинства
методов не сохранились.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x3C846352 / spBaseObject` | same |
| Registration / initializer | `0x0075FF40 / 0x006D3820` | `0x004A8EF0 / 0x004826A0` |
| Factory | `0x00460130` (protected) | `0x0015CE10` |
| Constructor | protected body | `0x0015CCF0` |
| Destructor / deleting destructor | `0x00460210 / 0x004604D0` | deleting `0x0015CC70` |
| RTTI clone / copy slot | `0x004601C0 / 0x0040ECE0` | `0x0015CD50 / 0x00100320` |
| Registration getter / vtable | `0x0045FE90 / 0x006E75BC` | `0x0015C410 / 0x0048E310` |
| Build component layout | `0x0045FEA0` | `0x0015C8E0` |
| Read / write / release | `0x00460300 / 0x00460400 / 0x0045FFF0` | `0x0015C610 / 0x0015C4C0 / 0x0015C8B0` |
| Independent deep copy | `0x00460240` (protected) | `0x0015CB30` |
| Exact size | observed complete `0x5C` | factory allocation `0x5C` |

Строка класса есть в обеих RTTI-таблицах. Пути `spVertexBuffer.cpp` или
заголовка нет, поэтому расположение реконструкции в `Code/Sparkplug` помечено
как inferred. PC diagnostics зато сохраняют три настоящих member spelling:
`m_uComponentFlags`, `m_uVertexCount` и `m_uFlags`.

## Layout

Обе платформы используют одинаковые offsets:

| Offset | Размер | Доказанная роль |
|---:|---:|---|
| `+0x10` | 2 | размер одной вершины в байтах |
| `+0x14` | 4 | `m_uComponentFlags`, маска layout |
| `+0x18` | 2 | число 32-битных компонент одной вершины |
| `+0x1C` | 4 | `m_uVertexCount` |
| `+0x20` | 4 | полный размер vertex data в байтах |
| `+0x24..+0x4F` | `22 × u16` | offsets компонент в 32-битных единицах |
| `+0x50` | 4 | `m_uFlags`, отдельные от component mask |
| `+0x54` | 4 | owned vertex-data pointer |
| `+0x58` | 1 | initialized |

PS2 constructor обнуляет `+0x10/+0x14/+0x18/+0x1C/+0x20/+0x54/+0x58` и
весь блок `+0x24..+0x4F`. PC constructor защищён, но все методы и полный
extent совпадают с этой раскладкой.

Release освобождает только `+0x54`, затем очищает pointer и initialized byte.
Маска, размеры, offsets и flags намеренно остаются наблюдаемыми. Отдельный raw
initializer также обнуляет `+0x14/+0x1C/+0x50`, но сохраняет старые
`+0x10/+0x18` и таблицу offsets при повторном использовании объекта. Этот
необычный stale-layout edge case покрыт тестом, а не «исправлен» молча.

## Разворачивание component mask

Алгоритм PC `0x0045FEA0` и PS2 `0x0015C8E0` совпадает буквально. Базовая XYZ
позиция всегда занимает три float: stride начинается с `12`, число компонент —
с `3`, а первый offset `+0x24` остаётся нулём.

| Bit | Offset field | Добавляется float | Корпусное подтверждение |
|---:|---:|---:|---|
| `0x000001` | `+0x26` | 1 | exact width, смысл не назван |
| `0x000002..0x000010` | `+0x28..+0x2E` | по 1 | четыре skin-weight word у `*3E/*7E` |
| `0x000020` | `+0x30` | 1 | packed palette indices у skin layouts |
| `0x000040` | `+0x32` | 3 | normal XYZ |
| `0x000080` | `+0x34` | 1 | exact width, смысл не назван |
| `0x000100` | `+0x36` | 1 | packed diffuse ARGB |
| `0x000200` | `+0x38` | 1 | exact width, смысл не назван |
| `0x000400` | `+0x3A` | 3 | exact width, смысл не назван |
| `0x000800..0x040000` | `+0x3C..+0x4A` | по 2 | восемь UV-like channels; UV0/UV1 подтверждены корпусом |
| `0x080000` | `+0x4C` | 3 | exact width, смысл не назван |
| `0x100000` | `+0x4E` | 3 | exact width, смысл не назван |

Например, `0x0840` даёт XYZ + normal + UV0: восемь float и stride `32`.
`0x093E` даёт четыре weight word, packed indices, diffuse и UV0: одиннадцать
32-битных компонент и stride `44`. Это независимо совпадает с PC SMO-корпусом.
Названия normal/diffuse/UV являются подтверждённой serialized/runtime
семантикой конкретных битов, но исходное имя engine enum всё ещё неизвестно.

## Инициализация и stream grammar

Обычная инициализация записывает component mask, vertex count и flags,
перестраивает layout, вычисляет `vertexSize = stride * vertexCount`, выделяет
данные и ставит initialized. Native allocation фактически использует
`componentCount * vertexCount * 4`; построитель layout гарантирует равенство
этих формул.

PC `0x00460300/0x00460400` и PS2 `0x0015C610/0x0015C4C0` читают и пишут:

```text
u32 componentFlags
u32 vertexCount
u32 flags
byte vertexData[vertexStride * vertexCount]
```

Каждая header/payload операция проверяется отдельно. Переносимый код сохраняет
формат, но добавляет overflow/allocation checks, чтобы повреждённый файл не
вызывал неконтролируемое выделение памяти.

## Два разных вида копирования

Как и у `spIndexBuffer`, RTTI clone создаёт blank buffer и вызывает только
root copy slot. Он не переносит component state и bytes. Полное копирование —
отдельный helper PC `0x00460240` / PS2 `0x0015CB30`: он воспроизводит layout,
выделяет новый `+0x54`, переносит `m_uFlags` и копирует payload. Именно этот
helper вызывает `spMeshData`.

Переносимая реализация разделяет эти операции как `Clone()` и
`CopyBufferForAnalysis()`. Она хранит bytes в `std::vector<std::byte>` и не
выдаёт host layout за 32-битный ABI; точный layout остаётся в двух
`SparkplugAbi.h`.

Открыто: original header/TU/API и component-enum spellings, семантика bits
`0x1/0x80/0x200/0x400/0x80000/0x100000`, роль `m_uFlags`, назначение raw и
external-storage initializers, точные error objects и поведение deep-copy
boolean параметра PS2. Эти вопросы не меняют уже доказанные layout, wire
format и ownership boundary.
