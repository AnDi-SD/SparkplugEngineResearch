# Нативный класс `spPS2Mesh`

Дата проверки: 2026-09-05. Статус: PS2-only identity, точный layout,
ownership подготовленных данных, conversion/attach/release и связь с renderer
подтверждены. Низкоуровневый packet-emitter сохранён как непрозрачная граница.

## Идентичность и наследование

`spPS2Mesh` имеет class ID `0x35ED77A5` и напрямую наследует
`spRenderMesh` (`0x67974A9C`). В PC `WinxClub.exe` строка класса отсутствует:
это не cross-platform ресурс, а PS2 runtime-лист.

| Факт | PS2 |
|---|---:|
| registration / initializer | `0x004B7380 / 0x00485090` |
| class string / base registration | `0x0045B958 / 0x004A8E90` |
| factory | `0x001EF6E0` |
| constructor/factory allocation | exact `0x58` |
| destructor | `0x001EF490` |
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
|---:|---|
| `+0x50` | owned подготовленный `spPS2MeshData*` |
| `+0x54` | owned non-RTTI helper, отправляющий PS2 packet |

Factory обнуляет оба поля и получает helper через глобальный backend-manager.
Публичное исходное имя helper-типа не сохранилось, поэтому реконструкция не
подменяет его придуманным классом.

Conversion `0x001EF320` принимает общие index/vertex buffers, создаёт новый
`spPS2MeshData` и запускает уже исследованный packet builder. Attach
`0x001EF3C0` передаёт data helper-у и переносит два итоговых счётчика:

- `spPS2MeshData +0x30` → `spMesh +0x4C`: emitted vertex count;
- `spPS2MeshData +0x34` → `spMesh +0x48`: primitive count.

Эти роли независимо подтверждаются builder-ом `0x0015F900`: primitive count
берётся из `spIndexBuffer +0x18`, а emitted vertex count вычисляется по
выбранному primitive path. Renderer-side `0x001FF8F0` следует по `+0x50` и
вызывает три операции helper-а для подготовки/отправки packet.

Native clone намеренно пустой: новый leaf создаётся, регистрируется в clone
manager и проходит success-only copy stub. Имя и payload не копируются. Такое
поведение сохранено в portable классе и отдельно проверяется тестом.

## Переносимый срез и проверка

`Sparkplug/Code/Sparkplug/spPS2Mesh.*` восстанавливает RTTI/factory, ownership
подготовленных данных, перенос счётчиков, release и blank-clone семантику.
Метод `AttachPreparedDataForAnalysis` является явно аналитическим безопасным
швом: он не пытается исполнять PS2 DMA/VIF/GIF packets на host.

`Sparkplug/Analysis/PS2/SparkplugAbi.h` фиксирует exact layout `0x58`, а CTest
проверяет offsets, RTTI, ownership, счётчики, release и clone. Read-only scanner
`python research/inspect_ps2_mesh.py` проверяет 24/24 опор в pristine PC/PS2
исполняемых файлах.

Открыты original header/TU и имена public API, настоящий тип packet-emitter-а,
его manager/ownership contract и полная семантика трёх draw operations. До их
доказательства platform packet execution остаётся evidence-only.
