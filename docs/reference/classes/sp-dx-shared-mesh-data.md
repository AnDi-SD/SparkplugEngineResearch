# spDXSharedMeshData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXSharedMeshData](../../../Sparkplug/Code/SparkplugDX/spDXSharedMeshData.h).

Статус: PC-only class identity, ownership, наблюдаемый layout, создание буферов и
полный wire-формат подтверждены. PS2 использует другой backend; имя класса там
отсутствует.

## Идентичность и исходный модуль

- class ID: `0x293A2681`;
- direct base: `spBaseObject`;
- registration: `0x007646A8`, initializer `0x006D56F0`;
- primary vtable: `0x006F23A0`;
- реализация: `Z:\Sparkplug\Code\SparkplugDX\spDXSharedMeshData.cpp`.

| Offset | Наблюдаемая роль |
| ---: | --- |
| `+0x00..+0x0F` | `spBaseObject` |
| `+0x10` | неизвестное слово |
| `+0x14` | intrusive `spDXIndexBuffer*` |
| `+0x18` | intrusive `spDXVertexBuffer*` |

Функция `0x004C29C0` получает размеры и адреса index/vertex payload. Она создаёт
index wrapper с `usage=8`, `D3DFMT_INDEX16 (0x65)`, `pool=1`, затем vertex wrapper
с `usage=8`, `FVF=0`, `pool=1`; оба блока копируются полностью через Lock/Unlock.
Clone `0x004C2950` оставляет буферы пустыми.

## `spDXSharedMeshDataSerializer`

- class ID `0x506F8A8C`, direct base `spSerializer`;
- runtime source class `spDXCombinedVB` (`0x4B18E622`);
- resolved load target `spDXSharedMeshData` (`0x293A2681`);
- primary vtable `0x006F2108`, stream-interface vtable `0x006F20FC`;
- write `0x004C1ED0`, read `0x004C1FA0`.

Wire-порядок точен:

```text
u32 indexByteSize
u32 vertexByteSize
u8  indexData[indexByteSize]
u8  vertexData[vertexByteSize]
```

Writer читает эти данные из `spDXCombinedVB` с указателями `+0x2C/+0x30` и
размерами `+0x34/+0x38`. Это не layout самого `spDXSharedMeshData`. Override
`0x004C1DE0` игнорирует входной ID и при загрузке заменяет source class на
`spDXSharedMeshData`. Reader берёт raw pointers из contiguous memory stream и
вызывает target `Init`.

Открыто: роль `+0x10`, original header/API, полный layout и ownership
`spDXCombinedVB`, shared payload внутри целого runtime save/load graph,
другие Init failures и device-loss/reset lifetime.
