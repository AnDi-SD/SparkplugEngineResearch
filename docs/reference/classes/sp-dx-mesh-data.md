# spDXMeshData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXMeshData](../../../Sparkplug/Code/Sparkplug/spDXMeshData.h), [spMeshData](../../../Sparkplug/Code/Sparkplug/spMeshData.h).

| Факт | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x3178114C / spPlatformSpecificMeshData` | same |
| RTTI clone | `0x00472630` | `0x0015CFF0` |
| Размер | proven prefix `0x20`, final size unknown | exact allocation `0x44` |

Original path самого data-класса строками не сохранился, поэтому
`Code/Sparkplug/spDXMeshData.*` остаётся inferred. Связанный serializer имеет
точный PC source path
`Z:\Sparkplug\Code\Sparkplug\spDXMeshDataSerializer.cpp`; это подтверждает
границу общего модуля, но не является прямым доказательством пути класса.

## Layout и ownership

Обе платформы независимо доказывают следующий prefix:

| Offset | Роль |
| ---: | --- |
| `+0x00..+0x13` | `spPlatformSpecificMeshData` |
| `+0x14..+0x17` | неизвестное слово |
| `+0x18` | owning `spIndexBuffer*` |
| `+0x1C` | owning `spVertexBuffer*` |

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

Это не наследование от `spMeshData` и не sharing тех же buffers. Leaf хранит собственную platform-specific копию двух общих buffer-объектов.

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

## Границы описания

Открыты:
