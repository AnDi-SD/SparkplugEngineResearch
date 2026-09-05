# Нативный класс `spDXMeshData`

Дата разбора: 2026-09-04. Статус: identity, hierarchy, PS2 exact layout,
общий owning-buffer prefix, destruction, blank RTTI clone и преобразование из
`spMeshData` подтверждены. Финальный PC `sizeof` и роли platform tail пока не
извлечены из защищённой `.rld` factory.

| Факт | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x3178114C / spPlatformSpecificMeshData` | same |
| Registration / initializer | `0x00760760 / 0x006D3C70` | `0x004A8F50 / 0x004826E0` |
| Factory | `0x004725C0` (`.rld` thunk) | `0x0015D0C0` |
| Constructor | protected body | `0x0015CFB0` |
| Destructor | `0x00472680`, deleting `0x004726F0` | `0x0015CF00` |
| RTTI clone | `0x00472630` | `0x0015CFF0` |
| Registration getter | `0x00472580` | `0x0015CEA0` |
| Vtable | `0x006E8F54` | `0x0048E340` |
| Размер | proven prefix `0x20`, final size unknown | exact allocation `0x44` |

Original path самого data-класса строками не сохранился, поэтому
`Code/Sparkplug/spDXMeshData.*` остаётся inferred. Связанный serializer имеет
точный PC source path
`Z:\Sparkplug\Code\Sparkplug\spDXMeshDataSerializer.cpp`; это подтверждает
границу общего модуля, но не является прямым доказательством пути класса.

## Layout и ownership

Обе платформы независимо доказывают следующий prefix:

| Offset | Роль | Evidence |
|---:|---|---|
| `+0x00..+0x13` | `spPlatformSpecificMeshData` | общий RTTI base и exact `0x14` base layout |
| `+0x14..+0x17` | неизвестное слово | не инициализируется constructor-ом и не читается доказанным conversion path |
| `+0x18` | owning `spIndexBuffer*` | оба destructor-а вызывают deleting virtual; PS2 conversion пишет deep copy |
| `+0x1C` | owning `spVertexBuffer*` | то же |

PS2 factory выделяет `0x44` байта, поэтому `+0x20..+0x43` существуют, но их
назначение пока неизвестно. Ни constructor, ни clone, ни conversion
`0x0015CEB0` этот tail не заполняют. На PC destructor доказывает prefix до
`+0x1F`, однако protected factory не позволяет честно объявить `0x20` полным
размером класса.

PS2 constructor вызывает `spPlatformSpecificMeshData` constructor и заменяет
только vptr. Как и у `spMeshData`, нативный free-list allocator не гарантирует
zero-fill. Следовательно, buffer fields и весь tail до отдельной
инициализации формально могут содержать старые данные. Portable-класс
намеренно использует безопасные `nullptr` owners и не воспроизводит этот
undefined-lifetime дефект.

## Преобразование из общей mesh data

PS2 `0x0015CEB0` принимает destination `spDXMeshData` и source `spMeshData`:

1. берёт source `spIndexBuffer*` из `+0x50`;
2. вызывает доказанный deep-copy helper `0x00159780` и пишет результат в
   destination `+0x18`;
3. берёт source `spVertexBuffer*` из `+0x54`;
4. вызывает deep-copy helper `0x0015CB30` и пишет результат в destination
   `+0x1C`;
5. возвращает `true`.

Это не наследование от `spMeshData` и не sharing тех же buffers. Leaf хранит
собственную platform-specific копию двух общих buffer-объектов. Нативная
функция рассчитана на свежий destination и не содержит rollback/null checks;
portable `InitializeFromMeshDataForAnalysis` сначала создаёт обе копии и лишь
затем атомарно заменяет owners.

RTTI clone устроен иначе. Он выделяет новый экземпляр (`0x44` на PS2),
регистрирует clone pair и вызывает только inherited name-copy slot. Buffer
payload не переносится. Тесты поэтому отдельно проверяют conversion, blank
clone и release.

## Связь с serializer

`spDXMeshDataSerializer` имеет общий class ID `0x77006ABE` и base ID
`0x66380037`. PC registration находится по `0x0075E338`, PS2 — по
`0x004A9190`.

PC cross-platform load `0x00429A40` читает четыре 32-битных значения и bool,
создаёт `spIndexBuffer`/`spVertexBuffer`, загружает оба payload и передаёт их в
доказанный init slot `spMeshData +0x1C`. Это исправляет раннюю ошибочную
трактовку: вызов `vtable +0x1C` принадлежит destination `spMeshData`, а не
`spDXMeshData`.

Обратный PS2 path `0x001618A0` создаёт на стеке exact `0x44`-байтный
`spDXMeshData`, вызывает `0x0015CEB0`, записывает FVF/component flags, vertex
count, рассчитанные размеры и 16/32-bit index marker, после чего сериализует
обе независимые buffer-копии. Это прямое доказательство назначения leaf-а как
промежуточной DX-формы в cross-platform pipeline даже внутри PS2 build.

## Реализованный срез и открытое

Portable-класс сохраняет native RTTI hierarchy, name-only clone, два concrete
owned buffer и транзакционное преобразование из уже восстановленного
`spMeshData`. ABI evidence остаётся раздельным: PC объявляет только proven
`0x20` prefix, PS2 — exact `0x44` layout с opaque tail.

Открыты:

- прямой PC `sizeof` и constructor body за `.rld`;
- роль `+0x14` и PS2 `+0x20..+0x43`;
- original header/TU и имена conversion/release API;
- точное значение всех четырёх cross-platform header words;
- проверка, использует ли active PC renderer tail напрямую или он нужен только
  для ABI/serialization compatibility.
