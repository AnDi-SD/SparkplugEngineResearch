# Семейство `spRenderer`

Дата проверки: 5 сентября 2026 года. Статус: identity, наследование, точные
границы объектов, lifecycle кэшей и платформенный интерфейс подтверждены для
PC/PS2; model-to-mesh submission доказан, имена и сигнатуры остальных
render-операций пока открыты.

[PC checkpoint14](native-pc-scene-render-runtime.md) добавляет original
DX4B0A90 device-state cache entryE4F4+4*index: equal suppresses call, changed
invokes COMvE4 and caches value **даже при HRESULT failure**. Это иной кэш,
чем commonC868 ниже. Перенесён только explicit single-entry source helper;
960 differential fields/96 двухшаговых cases,6 C++ tests. Полная длина массива,
его startup и GPU не выводятся из этой узкой проверки.

## Доказанная иерархия

Исполняемые файлы дают две независимые ветки одной общей границы:

```text
spCrossPlatform (0x20A72504)
└─ spRenderer (0x2D9C0296, abstract registration)
   ├─ spDXRenderer (0x46004EE1, PC, abstract registration)
   │  └─ spPCRenderer (0x26267C84, concrete factory)
   └─ spPS2Renderer (0x303652B8, PS2, concrete factory)
```

`spRenderer` и `spDXRenderer` имеют null RTTI factory. Конечные
`spPCRenderer` и `spPS2Renderer` создаются своими factory и реализуют clone
через создание нового объекта того же leaf-типа. На PS2 строка
`spDXRenderer` отсутствует, поэтому DX-ступень нельзя вставлять в общую
иерархию.

| Факт | PC | PS2 |
|---|---:|---:|
| `spRenderer` registration / initializer | `0x0075F8F0 / 0x006D35E0` | `0x004A9C70 / 0x00482F50` |
| `spRenderer` getter | `0x004562F0` | `0x00179560` |
| `spRenderer` destructor | `0x004561E0` | `0x0017A160` |
| `spRenderer` state-cache invalidator | `0x00454940` | `0x00179E60` |
| platform leaf registration / initializer | `0x00764A10 / 0x006D58B0` | `0x004B7BA0 / 0x00485350` |
| platform leaf factory | `0x004C5AB0` | `0x001FCB80` |
| platform leaf constructor | protected entry `0x004C5A10` | `0x001FC2E0` |
| platform leaf destructor | `0x004C5A80` | `0x001FC1B0` |
| platform leaf clone | `0x004C5B10` | `0x001FCA70` |

Для промежуточного PC-класса `spDXRenderer` registration находится по
`0x007639C0`, initializer по `0x006D50D0`, destructor по `0x004AE170`, а
protected constructor entry по `0x004AE370`. Тела constructor-а и части
lifecycle закрыты SecuROM `.rld`; окружающий ABI зафиксирован, но защищённый
код не объявляется восстановленным.

Точный PC original source anchor семейства:
`Z:\Sparkplug\Code\SparkplugDX\spDXRenderer_Init.cpp`. На PS2 отдельно
сохранился basename `spPS2Renderer.cpp`, но не полный путь. Пути общих и
platform-leaf заголовков в дереве реконструкции помечены как inferred.

## Точные границы и общий prefix

На обеих платформах объект начинается одинаково:

| Offset | Роль |
|---:|---|
| `+0x00` | primary vptr унаследованной object-иерархии |
| `+0x14` | отдельный support/singleton subobject vptr |
| `+0x18` | vptr платформенного renderer-интерфейса |
| `+0xC050` | байт, разрешающий постановку render-команд |

Дальше layout расходится:

| Граница | PC | PS2 |
|---|---:|---:|
| exact `sizeof(spRenderer)` | `0xCA08` | `0xCBB0` |
| render-state cache | `12 × u32` по `+0xC868` | `12 × u32` по `+0xC9D0` |
| texture-state cache | `72 × u32` по `+0xC898` | `96 × u32` по `+0xCA00` |
| exact concrete renderer size | `0xF368` | `0x19D00`, align `16` |

PC-граница `0xCA08` следует из destructor-а, который уничтожает последний
`0x40`-байтный member по `+0xC9C8`, и начала derived lifetime. PS2 constructor
общего класса пишет вплоть до `+0xCBA8`, после чего `spPS2Renderer` начинает
первое derived-поле с `+0xCBB0`; factory непосредственно выделяет `0x19D00`
байт с выравниванием `16`.

Оба invalidator-а заполняют свои render- и texture-state массивы значением
`~0u`. Это именно размеры нативных кэшей, а не оценка по соседним данным.
Portable-класс сохраняет наблюдаемое поведение, но хранит кэши в безопасных
host-контейнерах; byte-exact layout живёт отдельно в `Analysis/PC` и
`Analysis/PS2`.

## Платформенный интерфейс из 29 операций

У `spRenderer+0x18` на обеих платформах ровно 29 callable slots. PC-таблицы:

| Класс | Адрес interface table |
|---|---:|
| `spRenderer` | `0x006E6F00` |
| `spDXRenderer` | `0x006EF9C8` |
| `spPCRenderer` | `0x006F28A0` |

Конечная PC-таблица отличается от DX-таблицы только slot `2`: вместо pure
entry `0x0060DB76` leaf ставит `0x004A1BF0`, возвращающий ноль. Полный порядок
29 адресов хранится в PC ABI header и проверяется сканером.

На PS2 common interface table начинается заголовком `0x0048ED80`, а leaf
table — двухсловным нулевым заголовком `0x00491950`; callable entries начинаются
по `0x00491958`. Все 29 leaf-entry являются отдельными MIPS thunk-ами:

1. безусловно переходят в concrete body;
2. в delay slot уменьшают interface-указатель `this` на `0x18`;
3. тем самым передают body адрес полного `spPS2Renderer`.

Это независимо подтверждает положение interface subobject, число операций и
29 concrete PS2 body. Имена методов не выводятся лишь из порядка vtable:
восстановление API продолжится от call sites, аргументов, D3D9/PS2 endpoints и
диагностических строк.

## Slot 9: передача mesh в backend

`spModel` закрывает чистый render-slot `spRenderable` и на обеих платформах
выполняет один и тот же протокол: pre-render, вызов renderer interface slot `9`
с `baseMesh`, post-render. Поэтому `SubmitMesh` в portable API — явно
аналитическое имя подтверждённой операции, а не заявление об оригинальном
C++ spelling.

| Граница | PC | PS2 |
|---|---:|---:|
| `spModel` render body | `0x00479DC0` | `0x0015A640` |
| renderer slot `9` body | `0x004BC670` | `0x001FF6A0` |
| platform mesh field | `spModel+0x58` | `spModel+0x50` |

PC body распаковывает подтверждённые draw-поля `spDXMesh` по `+0x44..+0x84`
и передаёт их в `0x004BC4A0`. Последняя функция в pristine executable является
SecuROM `.rld` bridge `FF 25 A4 14 3B 01`; скрытое тело и его внутренние имена
не объявляются восстановленными. Однако открытый downstream wrapper
`0x004BC290` достоверно достигает D3D9 declaration/FVF, stream/index и
`DrawPrimitive`/`DrawIndexedPrimitive` endpoints.

PS2 body не защищён: он идёт через `mesh+0x50 -> payload+0x40`, подготавливает
VIF/DMA/GS state и связывает buffer references `mesh+0x4C/+0x48` через
`0x00208D10/0x00208CE0`. Это независимая платформенная опора того же
высокоуровневого slot `9`.

## Важная поправка про D3D device

PC interface-функции получают `this = complete object + 0x18`. Чтение по
`interface this + 0xC9D0` поэтому обращается к адресу полного объекта
`+0xC9E8`. Этот адрес действительно несёт D3D device pointer в найденных draw
callers, но он попадает в поздний storage общего `spRenderer`, а не доказывает
начало полей `spDXRenderer`. Запись вида «derived field `spDXRenderer+0xC9E8`»
теряла multiple-inheritance adjustment и теперь не используется.

## Перенесённый срез и проверка

В `Sparkplug/Code` восстановлены:

- abstract common owner и singleton `spRenderer`;
- PC abstract boundary `spDXRenderer`;
- concrete `spPCRenderer` и `spPS2Renderer` с native class IDs/factories;
- platform-разные размеры state caches и безопасная операция invalidation.

Намеренно не перенесены: неподтверждённые имена 29 методов, D3D device
ownership/reset, PS2 GS state, большая непрозрачная часть layout и защищённые
PC constructor bodies.

`python research/inspect_renderers.py` выполняет 78 read-only проверок на
эталонных PC/PS2 executable: SHA-256, RTTI-цепочки, factories, allocation
sizes, cache loops, exact source path, полную PC interface table и все PS2
thunk/body связи. `python research/inspect_render_submission.py` добавляет 37
проверок цепочки render-node/model/slot-9/backend, включая полные хэши тел и
платформенную асимметрию model classifier. CTest отдельно проверяет portable
factory/singleton/cache поведение, slot mapping и ABI `static_assert`.

## Slot 26: применение `spFog`

Pre-render path `spRenderable` связывает fog с callable slot `26`. PC body
`0x004AD390` читает `spFog+0x14..+0x24` и выставляет D3D9 fog enable/color/mode,
start/end/density states. PS2 body `0x001FB1F0` сохраняет fog relationship и
выделяет linear mode. В PS2 caller операция видна как byte offset `+0x70`, но
первые восемь байт таблицы — GCC vtable header: `(0x70-8)/4 = 26`. Slot `28`
ведёт в shutdown body и fog-операцией не является. Детали и отдельные 42
regression-проверки собраны в [`spFog`](native-class-sp-fog.md).

## Texture transform: PC 4×4 slot23 and UV 3×3 slot24

PS2 material path называет ещё одну общую операцию. `spMaterialTexture`
`0x001732E0` передаёт texture-stage index и 3×3 UV matrix с `+0x48` через
secondary offset `+0x64`, то есть slot `(0x64-8)/4 = 23`. PS2 body
`0x001FAB00` преобразует её во временную 4×4 matrix и вызывает внутренний
backend virtual `+0x124`. Аналитическое имя `SetTextureTransform` добавлено в
portable slot map; original C++ spelling пока не утверждается.

PC slot `23` — открытое тело `0x004BB590` размером `0xB4`. Оно копирует
64-байтную matrix в per-stage cache, прибавляет к stage `0x10`
(`D3DTS_TEXTURE0`) и вызывает D3D device virtual `+0xB0` (`SetTransform`).
Эти два slot23 НЕ имеют одинаковую форму аргумента: PC принимает4×4,
PS2 material caller передаёт3×3. CP22/23 исправляет прежний слишком общий
вывод о тождестве операций. Реальный PC material caller467B70 использует
slot24/4BB4B0 для3×3. Он встраивает её в верхний левый3×3 блок4×4 (последняя
диагональ1), кеширует по `renderer+F0F4+64*stage` и передаёт device+ B0.
Оба PC варианта отправляют matrix каждый раз и игнорируют HRESULT.
Аналитический `SetUVTransform3x3` учитывает PC24/PS2 23; прежний slot23
сохранён как отдельная 4×4 PC операция, без переноса PC сигнатуры на PS2.

## Следующая логическая граница

Для нативного импортера/экспортера важнее всего раскрыть PC operations,
которые:

- создают и освобождают platform texture/vertex/index resources;
- преобразуют mesh component flags в backend declaration;
- связывают material/pass state с уже доказанным mesh submission;
- переживают device loss/reset.

Локальные участки `spMaterialData`, fog binding и runtime pass/layer ownership
уже описаны; это не означает100% классов или завершённый end-to-end pipeline.
PS2 pre-render теперь также доказан как запись material/fallback pointer и
связанного float в renderer current-material cache, тогда как fog применяется
сразу через slot `26`; material state читается позднее draw path. Следующий
проход идёт от этого cache consumer-а к уже найденным D3D9/PS2 mesh endpoints.
Spatial/gameplay ветки не исключаются, но исследуются только когда оказываются
обязательными владельцами или consumers этого графического пути.

## PC продолжение — checkpoint11, 2026-09-06

[Original queue/protocol evidence](native-pc-renderer-protocol.md): alpha
fixed2048×24, borrowed lifetime, wrapped priority/distance/particle sorting,
original general vector20/sort/clear и nine mode vectors8 выполнены. PC nine
pass slots20..40 — shared5B7A00 no-op; эту ветку не выдавать за рабочий PC draw.
Both flush paths игнорируют support/model failure. Nonempty callbacks и shared
material-save byte7400FC проверены отдельно. Native75+64/static28, source
callback/math23, differential768, CTest15/15. Alpha qsort — explicit seam для
preordered input, tie order неизвестен; normal456090 sort исполняется целиком.

Renderer ABI уточнён, portable `spRenderable` callbacks добавлены; **полного
source renderer queue/Scene/backend подключения ещё нет**. Full PC factory
scout остановлен на100k/2s protection cap; лимит не повышался. Runtime global
75F8E8 initialization, remaining backend states и SceneInit/Partition/Occlusion
остаются обязательными зависимостями, не заменяются recording successes.
