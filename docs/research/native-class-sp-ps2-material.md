# Нативный класс `spPS2Material`

Дата проверки: 2026-09-05. Статус: PS2-only identity, direct base, exact
layout, обе vtable, color/power accessors, update, copy и clone подтверждены.

## Идентичность

`spPS2Material` имеет class ID `0x0F507BC8` и напрямую наследует
`spMaterial` (`0x5C0314C5`). Строка класса присутствует только в PS2
executable; PC содержит `spPS2MaterialDataSerializer`, но не сам runtime-класс.

| Факт | PS2 |
|---|---:|
| registration / initializer | `0x004B79C0 / 0x00485210` |
| class string / base registration | `0x0045BA78 / 0x004A9610` |
| factory | `0x001F2FC0` |
| registration getter / copy | `0x001F20A0 / 0x001F20B0` |
| destructor / clone | `0x001F2430 / 0x001F24A0` |
| material update | `0x001F2360` |
| primary/interface vtable | `0x004916E0 / 0x00491704` |
| размер | exact allocation `0xD0` |

Primary header содержит девять слов и ссылается на собственные destructor,
clone, copy и RTTI getter. Interface table содержит два header-слова и 22
операции: одиннадцать общих material-state thunks, update, четыре пары
RGBA get/set и пару specular-power set/get.

## Layout и defaults

Factory выделяет ровно `0xD0`, вызывает общий `spMaterial` constructor и
заменяет обе vtable. Tail совпадает по offsets с concrete material payload:

| Offset | Поле |
|---:|---|
| `+0x80` | diffuse RGBA |
| `+0x90` | ambient RGBA |
| `+0xA0` | specular RGBA |
| `+0xB0` | emissive RGBA |
| `+0xC0` | specular power |

Четыре цвета factory получает из двух глобальных packed black/white значений:
diffuse/specular — white, ambient/emissive — black. `+0xC0` factory явно не
записывает. Поэтому нулевой `specularPower` переносимой реализации — безопасная
host-side инициализация, а не заявление о доказанном native default.

## Copy, clone и update

В отличие от success-only copy stub у `spMaterialData`, функция `0x001F20B0`
сначала копирует общий `spMaterial`, затем все 17 float от `+0x80` до `+0xC0`.
Clone `0x001F24A0` создаёт default `spPS2Material`, регистрирует пару в clone
manager и вызывает именно этот полноценный copy path.

Update `0x001F2360` сравнивает cached renderer generation в `spMaterial +0x78`
с текущим значением. При изменении либо forced-вызове он обновляет color
controller, если тот активен, и проходит все material passes, вызывая
`0x001700B0(pass, index)`. Это первый подтверждённый consumer pass-графа перед
platform renderer state application.

## Реконструкция и проверка

`Sparkplug/Code/Sparkplug/spPS2Material.*` восстанавливает RTTI/factory,
проверяемый color/power payload и полноценный clone/copy. Exact layout и адреса
зафиксированы в `Sparkplug/Analysis/PS2/SparkplugAbi.h`; CTest проверяет defaults,
общий state и копирование хвоста.

`python research/inspect_ps2_material.py` даёт 31/31 read-only проверок pristine
PC/PS2 executable: hashes, platform presence, регистрацию, тела, обе vtable,
allocation, offsets, copy range и pass-to-texture-transform dispatch, включая
PC D3D9 endpoint.

Открыты original header/TU/API, native начальное значение `+0xC0`, исходные
имена update/generation state, связь выбора с `spPS2MaterialDataSerializer` и
остальные texture/render-state operations. `0x001700B0` теперь подтверждён как
per-layer stage dispatch к renderer texture-transform slot `23`.
