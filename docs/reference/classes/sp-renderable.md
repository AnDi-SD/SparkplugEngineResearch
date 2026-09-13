# spRenderable

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spRenderable](../../../Sparkplug/Code/Sparkplug/spRenderable.h).

## Идентичность и граница

`spRenderable` имеет class ID `0x4FDA4542` и напрямую наследует
`spNamedObject` (`0x44DE07FD`) в обоих исполняемых файлах. RTTI factory и
property callback отсутствуют. Исходный header/translation unit не найден;
`Sparkplug/Code/Sparkplug/spRenderable.*` является явно inferred путём.

| Факт | PC | PS2 |
| --- | ---: | ---: |
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

| Offset | Роль |
| ---: | --- |
| `0x14` | derived runtime/render mode cache |
| `0x18` | `AlphaSortEnable` |
| `0x1C` | `Priority` |
| `0x20` | material relationship |
| `0x24` | fog relationship |
| `0x28` | PC debug-helper result, имя неизвестно |
| `0x2C/0x30` | direct callbacks |
| `0x34` | первая группа 8-byte callback records |
| `0x44` PC / `0x40` PS2 | вторая группа |
| `0x54/0x55` PC | включение групповых dispatch paths |

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

## Границы описания

Реконструкция сохраняет RTTI, null-clone, copy semantics и четыре serializer-visible свойства. Relationship facade всё ещё принимает `spBaseObject`, потому что это безопасная ownership-граница общей abstract family, а исходная C++ сигнатура setter-а не доказана.
