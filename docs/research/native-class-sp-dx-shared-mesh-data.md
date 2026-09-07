# `spDXSharedMeshData`: сериализованная пара общих DX-буферов

Статус: PC-only class identity, ownership, наблюдаемый layout, создание буферов и
полный wire-формат подтверждены. PS2 использует другой backend; имя класса там
отсутствует.

## Идентичность и исходный модуль

- class ID: `0x293A2681`;
- direct base: `spBaseObject`;
- registration: `0x007646A8`, initializer `0x006D56F0`;
- primary vtable: `0x006F23A0`;
- реализация: `Z:\Sparkplug\Code\SparkplugDX\spDXSharedMeshData.cpp`.

CP104 исполнил защищённые factories: `4C28E0` запросил ровно `0x1C` байт,
`4C1DF0` для serializer — `0x14`. Прежний наблюдаемый prefix класса теперь
совпадает с непосредственно измеренным размером allocation:

| Offset | Наблюдаемая роль |
|---:|---|
| `+0x00..+0x0F` | `spBaseObject` |
| `+0x10` | неизвестное слово |
| `+0x14` | intrusive `spDXIndexBuffer*` |
| `+0x18` | intrusive `spDXVertexBuffer*` |

Функция `0x004C29C0` получает размеры и адреса index/vertex payload. Она создаёт
index wrapper с `usage=8`, `D3DFMT_INDEX16 (0x65)`, `pool=1`, затем vertex wrapper
с `usage=8`, `FVF=0`, `pool=1`; оба блока копируются полностью через Lock/Unlock.
Clone `0x004C2950` оставляет буферы пустыми.

## `spDXSharedMeshDataSerializer`

Сериализатор собран в том же `.cpp`:

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

Нативная функция после нормально завершившегося вызова `Init` безусловно
возвращает `true`; CP104 подтвердил это отдельным seam с возвращаемым нулём.
Это **не означает**, что настоящий отказ создания буферов завершится возвратом:
при COM CreateIndexBuffer failure оригинал останавливается на чтении адреса
`0` в `4C2AB6`, внутри `Init`. Такой guest не возобновлялся.

При успешном чтении `4C1FA0` оставляет cursor после двух размеров: `8` для
110-байтного payload. Остальные 102 байта копируются через raw pointers,
без Read/Seek. Portable `ReadPayloadForAnalysis` остаётся последовательным
удобным reader; `ReadContiguousPayloadForAnalysis` отдельно сохраняет native
cursor для zero-origin memory stream. Оба проверяют сумму размеров, оставшийся
input и предел payload 32 МиБ **до** выделения векторов. Host возвращает ошибку
инициализации; contiguous helper явно отказывает nonzero origin/null buffer.
Его подключение к generic reference reader пока открыто: там требуется полное
потребление inline payload, а оригинальный shared reader ведёт себя иначе.

## Проверка

`research/inspect_dx_shared_mesh_data.py` выполняет 20 binary checks класса.
`research/inspect_dx_shared_mesh_data_serializer.py` выполняет ещё 27 проверок:
контрольные SHA, отсутствие PS2 leaf, bodies, обе vtable, IDs, offsets
промежуточного payload и точный порядок stream calls. CTest проверяет создание,
release, blank clone, wire round-trip и отказ на обрезанном payload.

[CP104: исполняемые проверки](native-pc-dx-shared-payload.md) добавил точные
байты, cursor, восемь error boundaries, отдельный Init-return seam и настоящий
Create failure stop. Все 22 allocations десяти завершённых случаев освобождены.

Открыто: роль `+0x10`, original header/API, полный layout и ownership
`spDXCombinedVB`, shared payload внутри целого runtime save/load graph,
другие Init failures и device-loss/reset lifetime.
