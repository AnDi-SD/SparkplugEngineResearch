# Семейство `spRenderTarget`

Дата проверки: 5 сентября 2026 года. Статус: RTTI-иерархия, основные ABI-поля,
PC reset-путь, платформенные форматы и точные размеры PS2-объектов подтверждены.
Имена типов внутренних texture-отношений и layer-target ветка пока открыты.

## Место в графическом конвейере

Исполняемые файлы доказывают три параллельные ветви:

```text
spResource (0x46F043FE)
└─ spRenderTarget (0x00D1229C, abstract)
   ├─ spDXRenderTarget (0x189A4642, PC, abstract)
   │  └─ spPCRenderTarget (0x0C681FC8, concrete)
   └─ spPS2RenderTarget (0x30A7021D, concrete)

spRenderTarget
└─ spCubeRenderTarget (0x0F8B095F, abstract)
   ├─ spDXCubeRenderTarget (0x5249684C, PC, concrete)
   └─ spPS2CubeRenderTarget (0x5AE83884, concrete no-op backend)

spCrossPlatform (0x20A72504)
└─ spRenderTargetManager (0x546C50F2, abstract)
   ├─ spPCRenderTargetManager (0x165C006F, concrete)
   └─ spPS2RenderTargetManager (0x01486E51, concrete)
```

Общие классы имеют null RTTI factory. Конечные platform-классы имеют factory;
у PC промежуточный `spDXRenderTarget` остаётся абстрактным, а
`spDXCubeRenderTarget` уже concrete. Это не симметричная искусственная
иерархия, а фактическая разница registration graph.

## Общий ABI

Обычная цель на обеих платформах имеет один и тот же доказанный префикс:

| Offset | Поле/роль |
|---:|---|
| `+0x00` | `spResource` (`0x14` байт) |
| `+0x14` | vptr отдельного target-интерфейса |
| `+0x18` | intrusive relationship на backing texture |
| `+0x1C` | width |
| `+0x20` | height |
| `+0x24` | `eTBPixelFormat` |

Размер этого common-префикса — `0x28`. `spCubeRenderTarget` добавляет ещё одно
texture-отношение по `+0x28`, поэтому его граница — `0x2C`.

PC `spDXRenderTarget` добавляет D3D surface pointer по `+0x28`; наблюдаемый
префикс равен `0x2C`. `spDXCubeRenderTarget` хранит шесть face surface pointers
по `+0x2C..+0x40`, а его наблюдаемая граница равна `0x44`. Защищённые `.rld`
factory не дают назвать эти PC-величины прямым `sizeof`, поэтому ABI-типы имеют
суффикс `ObservedPrefix`.

PS2 даёт прямые allocation constants:

| Класс | Размер |
|---|---:|
| `spRenderTarget` | `0x28` |
| `spCubeRenderTarget` | `0x2C` |
| `spPS2RenderTarget` | `0x30` |
| `spPS2CubeRenderTarget` | `0x2C` |
| `spRenderTargetManager` / PS2 leaf | `0x44` |

У `spPS2RenderTarget` по `+0x28` расположен handle texture manager, а байт
активности — по `+0x2C`. Cube leaf не добавляет storage.

## Разные таблицы форматов

Токен `eTBPixelFormat` подтверждён строкой `Invalid eTBPixelFormat.`. Имена
enumerator-ов не сохранились, поэтому реконструкция пока использует нейтральные
`Format0..Format5`.

| Backend | Допустимые значения | Нативное отображение |
|---|---|---|
| DX ordinary | `0, 1, 3, 4, 5` | `0x15, 0x16, 0x17, 0x1A, 0x19` |
| DX cube | `0, 1, 3, 4` | `0x15, 0x16, 0x17, 0x1A` |
| PS2 ordinary | `0, 1, 5` | `0, 1, 10` |

Значение `2` отвергается всеми разобранными реализациями; `6` используется
как invalid/uninitialized sentinel в Reinit-проверках. Нельзя применять одну
общую таблицу к обычной и кубической PC-цели: формат `5` является явным
контрпримером.

PS2 renderer содержит точную диагностику
`spCubeRenderTarget not supported on PS2`. Concrete PS2 cube class существует,
но его backend-операции являются успешными no-op; наличие RTTI/factory не
означает поддержку аппаратной cube render target.

## Потеря устройства и менеджер

На PC точный reset-путь выглядит так:

```text
spPCRenderer reset
  -> spRenderTargetManager::ReleaseTargetsForDeviceReset()
  -> D3D device reset
  -> spRenderTargetManager::ReinitTargetsForDeviceReset()
```

Менеджер содержит три разных 12-байтных container state:

| Offset | Содержимое |
|---:|---|
| `+0x18` | ordinary render targets |
| `+0x24` | cube render targets |
| `+0x30` | layer render-target textures |
| `+0x3C` | поле, инициализируется нулём |
| `+0x40` | поле, инициализируется единицей |

PC body `0x0045CD10` проходит первые два списка и вызывает release-слот; затем
обрабатывает layer-list отдельным helper. Body `0x0045CE70` повторяет обход для
reinit. Строки диагностик отдельно называют `RenderTarget`,
`CubeRenderTarget` и `LayerRenderTarget`.

Удаление обычной цели (`0x0045CBC0`) и cube-цели (`0x0045CBF0`) не стирает
узел из списка: у найденного узла байт `+0x0C` меняется на ноль. Portable manager
сохраняет это наблюдаемое active/inactive поведение. Реализация третьего списка
намеренно отложена до разбора владельца layer texture.

## Original source anchors

На PC сохранились точные пути:

- `Z:\Sparkplug\Code\SparkplugDX\spDXRenderTarget.cpp`;
- `Z:\Sparkplug\Code\SparkplugDX\spDXCubeRenderTarget.cpp`.

На PS2 сохранился basename `spPS2RenderTarget.cpp`, но не полный путь. Поэтому
`Code/SparkplugPS2/spPS2RenderTarget.cpp` соответствует подтверждённому имени,
а расположение каталога остаётся inferred.

## Адреса ключевых операций

| Операция | PC ordinary | PC cube | PS2 ordinary |
|---|---:|---:|---:|
| `Init` | `0x004CDC70` | `0x004AC580` | `0x002098A0` |
| `ReinitTargetsForDeviceReset` | `0x004CDBE0` | `0x004AC4F0` | `0x00209A20` |
| `ReleaseTargetsForDeviceReset` | `0x004CDD60` | `0x004AC2A0` | `0x00209820` |
| interface table | `0x006F33F8` | `0x006EF80C` | leaf group `0x00491CC0` |

Обе PC interface tables содержат 13 записей. Их первые три slots точно
соответствуют `Init`, `ReinitTargetsForDeviceReset` и
`ReleaseTargetsForDeviceReset`; остальные имена не выводятся только из порядка.

## Перенесённый срез и проверка

В `Sparkplug/Code` восстановлены class identities, factories, common state,
format gates, ordinary/cube reset lifecycle и two-list часть manager. Реальные
D3D COM-объекты и PS2 texture-manager объект заменены безопасным backend-state
seam: это переносимое поведение, а не заявление о byte-exact host ABI.

`python research/inspect_render_targets.py` выполняет 43 read-only проверки на
эталонных PC/PS2 executable: SHA-256, class/source strings, hash окон методов,
обе 13-slot PC tables, PS2 vtable groups, factory sizes и inactive-node store.
CTest отдельно проверяет RTTI factories, ABI, format matrices, шесть cube faces
и полный release/reinit цикл.

## Что остаётся неизвестным

- исходный тип backing/cube texture relationships по `+0x18/+0x28`;
- original names/signatures десяти остальных target-interface slots;
- точный PC allocation size там, где factory закрыта `.rld`;
- класс и lifecycle третьего `LayerRenderTarget` списка;
- связь target creation с material passes и фактическими renderer slots 0/1;
- полные имена `eTBPixelFormat` enumerator-ов.

Следующая связная задача для нативного importer/exporter — идти от третьего
списка и backing texture к texture manager/material render-target texture, а
затем замкнуть вызовы renderer slots 0/1 на фактический render pass.
