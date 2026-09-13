# spPS2Mesh

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2Mesh](../../../Sparkplug/Code/Sparkplug/spPS2Mesh.h).

## Идентичность и наследование

`spPS2Mesh` имеет class ID `0x35ED77A5` и напрямую наследует
`spRenderMesh` (`0x67974A9C`). В PC `WinxClub.exe` строка класса отсутствует:
это не cross-platform ресурс, а PS2 runtime-лист.

| Факт | PS2 |
| --- | ---: |
| class string / base registration | `0x0045B958 / 0x004A8E90` |
| blank clone | `0x001EF530` |
| conversion из common buffers | `0x001EF320` |
| attach подготовленных данных | `0x001EF3C0` |
| release | `0x001EF440` |
| draw preparation consumer | `0x001FF8F0` |

Primary vtable header по `0x00491410` равен
`(0, 0, 0x001EF490, 0x00100810, 0x001EF530, 0x00105DC0,
0x001EF310, 0x00100010, 0x00100050)`. Secondary header по `0x00491434`
равен `(0, 0, 0x001EF790, 0x001EF780, 0x00159C80, 0x001EF3C0,
0x001EF3B0, 0x001EF320, 0x001EF440)`.

## Layout и время жизни

После точного `0x50`-байтного `spRenderMesh` класс добавляет два указателя:

| Offset | Роль |
| ---: | --- |
| `+0x50` | owned подготовленный `spPS2MeshData*` |
| `+0x54` | owned non-RTTI helper, отправляющий PS2 packet |

Factory обнуляет оба поля и получает helper через глобальный backend-manager.
Публичное исходное имя helper-типа не сохранилось, поэтому реконструкция не
подменяет его придуманным классом.

- `spPS2MeshData +0x30` → `spMesh +0x4C`: emitted vertex count;
- `spPS2MeshData +0x34` → `spMesh +0x48`: primitive count.

Эти роли независимо подтверждаются builder-ом `0x0015F900`: primitive count
берётся из `spIndexBuffer +0x18`, а emitted vertex count вычисляется по
выбранному primitive path. Renderer-side `0x001FF8F0` следует по `+0x50` и
вызывает три операции helper-а для подготовки/отправки packet.
