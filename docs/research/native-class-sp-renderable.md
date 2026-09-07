# Нативный класс `spRenderable`

Дата проверки: 2026-09-06. Статус: проверяемый общий срез; трёхфазный
pre/render/post протокол доказан, renderer callbacks и исходные имена
виртуальных методов ещё не восстановлены.

## Идентичность и граница

`spRenderable` имеет class ID `0x4FDA4542` и напрямую наследует
`spNamedObject` (`0x44DE07FD`) в обоих исполняемых файлах. RTTI factory и
property callback отсутствуют. Исходный header/translation unit не найден;
`Sparkplug/Code/Sparkplug/spRenderable.*` является явно inferred путём.

| Факт | PC | PS2 |
|---|---:|---:|
| registration | `0x0075E030` | `0x004AB230` |
| registration initializer | `0x006D2960` | `0x00483EC0` |
| registration getter | `0x00423E20` | `0x001A9270` |
| destructor body | `0x00423D60` | `0x001A9A00` |
| deleting destructor | `0x00423FB0` | ABI slot `0x00100810` |
| copy | `0x00423C70` | `0x001A9690` |
| vtable | `0x006DC9B0` | `0x00490330` |
| размер | `0x58` | `0x50` |

PS2 constructor `0x001A9AC0` даёт полную инициализацию: runtime mode `+0x14`
равен нулю, `AlphaSortEnable +0x18` равен единице, `Priority +0x1C` равен
нулю, relationships `+0x20/+0x24` пусты, callbacks `+0x2C/+0x30` пусты,
оба контейнера пусты и bytes `+0x4C/+0x4D` сброшены. Прежняя общая трактовка
`+0x28` как renderer-global float была слишком сильной: PC ctor
`423F10→13C7260` получает **raw DWORD через `spDebugManager::41D4E0`**
(lazy global `75526C`), не через renderer. PS2 здесь повторно не исследуется;
исходное имя поля и полный контракт debug sequence ещё не названы.

## Layout и platform-разница

Общий смысл полей совпадает, но объект нельзя описывать одним byte layout:

| Offset | PC | PS2 | Роль |
|---:|---|---|---|
| `0x14` | `u32` | `u32` | derived runtime/render mode cache |
| `0x18` | byte | byte | `AlphaSortEnable` |
| `0x1C` | `u32` | `u32` | `Priority` |
| `0x20` | pointer | pointer | material relationship |
| `0x24` | pointer | pointer | fog relationship |
| `0x28` | raw `u32` | ранее наблюдавшийся scalar | PC debug-helper result, имя неизвестно |
| `0x2C/0x30` | pointers | pointers | direct callbacks |
| `0x34` | 16-byte vector | 12-byte container | первая группа 8-byte callback records |
| `0x44` PC / `0x40` PS2 | 16/12 bytes |  | вторая группа |
| `0x54/0x55` PC | `0x4C/0x4D` PS2 |  | включение групповых dispatch paths |

PC `0x00423E30` и `0x00423EA0` проходят по 8-байтным callback records,
удаляют запись при результате `-1` и прекращают проход при `0`. PS2
`0x001A9290/0x001A93F0` реализуют тот же контракт, но container ABI занимает
12 байт: count находится в `+4`, storage pointer в `+8`, а исходная роль слова
`+0` ещё не названа. Это и создаёт сдвиг `0x08` у всех полей `spModel` на PC.
PC pre-group использует vector **`+0x44`**, включение **byte54**;
post-group — vector **`+0x34`**, включение **byte55**. Прежние ABI имена
enable-полей были переставлены; теперь исправлены.

Copy functions копируют inherited name, `+0x28`, alpha-sort, priority,
material и fog; runtime mode пересчитывается виртуальным slot `+0x34` PC /
`+0x3C` PS2. На PC базовый hook `423B60` обнуляет mode, но override Model
`48EAA0` — no-op: существующий destination mode сохраняется. Callback
pointers/lists не копируются этим проходом.

## Pre-render, чистый render-slot и post-render

PS2 vtable header `0x00490330` показывает чистый virtual по header offset
`+0x2C`. Это основной render-slot абстрактного `spRenderable`; concrete
`spModel` заменяет его на `0x0015A640`. Соседние bodies образуют проверяемый
протокол:

- `0x001A9840` (header `+0x28`) обрабатывает alpha-sort/material/fog и
  pre-callback groups; при немедленном проходе подготавливает renderer state;
- чистый `+0x2C` выполняет собственную отрисовку leaf-класса;
- `0x001A9790` (header `+0x30`) выполняет post-callback groups и восстанавливает
  renderer state.

Точный PS2 pre/post разбор уточняет material boundary. Pre-render не отправляет
весь material в backend напрямую: если renderer override не активен, он кладёт
material relationship `+0x20` (либо renderer fallback) в current-material cache
`renderer +0xC164`, а float `spRenderable +0x28` — в `renderer +0xC16C`.
Fog relationship `+0x24` отдельно уходит в renderer slot `26`. Если у material
установлен byte `+0x74`, pre-render сохраняет renderer byte `+0xC19C` и временно
обнуляет его; post-render `0x001A9790` восстанавливает значение. Следовательно,
material state потребляется позднее mesh-submission path через renderer cache,
а не теряется между pre-render и draw.

PC `spModel` vtable подтверждает ту же тройку по callable offsets
`+0x20/+0x24/+0x28`: `0x00423FD0`, `0x00479DC0`, `0x004240D0`. Это ещё не
даёт оригинальных имён или точных callback typedef, но уже отделяет общий
material-state protocol от leaf submission. В PC checkpoint10 direct callback
проверен как **cdecl(model, camera, support)**. Pre false даёт Model успешный
skip, mesh false пропускает post, post false даёт failure. Fog backend slot26
вызывается, но его return игнорируется. Renderer overrideC188 сохраняет caches
C18C/C194; иначе туда записываются material/fallbackC9C0 и raw model28.

## Переносимый срез и неизвестные

Реконструкция сохраняет RTTI, null-clone, copy semantics и четыре
serializer-visible свойства. Concrete [`spMaterialData`](native-class-sp-material-runtime.md)
и [`spFog`](native-class-sp-fog.md) теперь восстановлены вместе с exact
platform layouts. Relationship facade всё ещё принимает `spBaseObject`, потому
что это безопасная ownership-граница общей abstract family, а исходная C++
сигнатура setter-а не доказана.

Portable слой дополнен virtual sphere/min-max getters и правильным virtual
invalidation hook. [Checkpoint10](native-pc-model-render-world.md) содержит
native47/static23 и сквозную geometry/world проверку. Продолжение
[checkpoint11](native-pc-renderer-protocol.md) уже исполнило nonempty groups,
alpha queue и material6C save/restore/failure:75+64 native checks. Callback
phase/raw28 copy перенесены в класс,576 two-dispatch state fields совпали.
Full renderer source wiring всё ещё нет. Неизвестны исходные имена `+0x14/+0x28`, typedef имена callbacks
и полная renderer state-machine. Material/fog
payload, fog renderer slot `26`, runtime pass/layer ownership и UV-transform
slot `23` уже доказаны. Следующий обязательный узел — чтение current-material
cache внутри mesh submission и остальные texture/render-state transitions.
