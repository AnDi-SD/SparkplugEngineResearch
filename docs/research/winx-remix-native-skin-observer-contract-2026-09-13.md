# Winx Remix: контракт native Skin observer — 13 сентября 2026

Добавлен ограниченный наблюдатель палитры `spSkin`: оригинальная игра по-прежнему вызывает callbacks, рассчитывает world/palette и рисует меш. Адаптер только читает текущие поля в собственном mesh callsite, сверяет их общей арифметикой и пишет диагностику. CPU fixture — **56 PASS**, игра/GPU/эмулятор в этом checkpoint не запускались; подачи skinned-геометрии ещё нет.

[Manifest](../../research/winx-remix-native-skin-observer-contract-2026-09-13.json) фиксирует 109 hashes, источник `1496557EBF7648E73DECB3B3F75FC5E562CC20826F9A781549FCF927E2781588`, EXE `524ED6A48C7EADA7B97493FDAB867A07203C01771E65EE96E5F579038D28890E`, полный frozen snapshot `native-skin-source-tests/observer-v4` и прежние попытки.

## Подтверждённая точка наблюдения

| Граница | Исходный контракт |
|---|---|
| Skin primary | `6E8C5C`, slot9/+24 → `46A240`, `thiscall(camera,support)`, `ret8`; boolean только AL |
| Собственный mesh call | `46A364 call [edx+24]`, return address `46A367`; ECX=`renderer+18`, единственный аргумент — заново прочитанный `skin+58` |
| Palette | renderer `+C9B8` → `float[boneCount][16]`; count `+C9BC` записывается из `skin+64` на `46A34E`, после публикации world identity |
| Skin inputs | `+60` influence hint, `+64` bone count, `+68` borrowed Node pointers, `+6C` inverse-bind matrices |
| Bone world | `Node+74/+80/+8C` — текущие cached world position/scale/orientation, точно те поля, которые читает `461D70` |

`46A240` сначала вызывает pre `423FD0`; тот может поставить объект в alpha queue и вернуть AL0. На этой ветви mesh/palette не наблюдаются. Count сбрасывается в 0 только после успешных mesh и post `4240D0`; отказ не означает нулевой palette count. Поэтому вне текущего Skin scope этот счётчик не служит доказательством skinning. Camera/support сохраняются из двух аргументов Skin wrapper; mesh call сам их не передаёт.

[CP64](native-pc-skin-render.md), [CP73](native-pc-skin-mesh-generated-render.md) и [alpha queue CP83](native-pc-skin-alpha-queue.md) дают исходные producer/caller доказательства. Новая read-only PE-проверка сравнила **12 участков** pristine `3F022480…` и фактического debug `C27EA9DB…`: полный Skin render, его vtable/getter/setter/destructor, renderer mesh body/interface slots, DX mesh initialize/preparation/vtables, `426B00` и `461D70`. Все байты совпали. Это идентичность перечисленных регионов, не выполнение полного callgraph или всех derived bone classes.

## Реализация и проверка

[winx_native_skin_source.h](../../research/rtx-remix/winx_native_skin_source.h) предоставляет `Initialize`, `EndFrame`, `Capture(mesh,renderer,submission,returnAddress)`. Единственная native запись — проверенная замена Skin vtable slot. Wrapper вызывает original ровно один раз, сохраняет весь EAX и восстанавливает прежний TLS в `__finally`. Обычные и вложенные вызовы продолжают исполняться; вложенные scopes не получают provenance credit.

Capture допускает только RA `46A367`, текущие thread/frame/scene scope/registry mutation, существующую support→Skin связь, выбранную main camera и точные native identities. Начальный cohort костей — plain `spNode 6DC4F4` текущей сцены с чистыми low dirty flags. Проверяются 1…256 костей, конечность всех входов, count/pointers/ranges; после расчёта повторно читаются Skin, mesh, support, bones, inverse binds и original palette. Это заимствование на время вызова, не постоянный ownership token и не доказательство отсутствия неперехваченного ABA.

Используются существующие общие `node_math::Affine` и `Multiply4ForAnalysis`, без обновления игровых caches. Сравнение явно допускает `abs=1e-4 + rel=1e-5 × magnitude`; максимальные ошибки и число несовпавших float words сохраняются отдельно. Допуск не выдаётся за exact binary reconstruction. Изменение общего `spSkin::ComposePaletteMatrixForAnalysis` и его fractional native proof выполняются отдельным блоком root.

Fixture использует собственные literal ABI records, pointer slots и функцию original вместо native кода. Проверены некоммутирующий пример CP64, существенная ошибка и малая допустимая погрешность, full EAX/AL, repeated/reentrant calls, SEH restore, wrong callsite, identity/scene/Enabled/dirty/NaN/Inf/count guards, точный предел256 и отсутствие записи в Skin/Node/palette. Watchdog —30 с; отсутствие COM/Remix instance отдельно проверено.

Первый snapshot `observer-v1` прошёл55 checks до новой weighted-capture integration. `observer-v2` не собрался из-за отсутствующего нового shared header в snapshot wrapper. `observer-v3` выявил Windows `max` macro conflict в этом helper; его владелец исправил вызов `numeric_limits::max`. Эти build.log сохранены. Окончательный `observer-v4` прошёл56, включая явную проверку неизменности исходных данных. Поведенческая проверка weighted mapper не приписывается данной palette fixture.

## Raw weighted bytes: следующий стык

Перед `4AA000(this=mesh+14,indexObject,vertexObject,keepCPU)` обычный `spVertexBuffer` ещё содержит authored bytes: `+54` data, `+10` stride, `+14` component flags, `+1C` count и `+24` таблицу22 `u16` offsets в DWORD-единицах. Float weight words bits2/4/8/10 находятся по offsets[2…5]; bit20/offset[6] — четыре packed `u8` palette indices. Общий `BuildVertexBytesForAnalysis` сохраняет weights и остальные bytes; только packed indices разворачивает в четыре ненормализованных float, увеличивая stride на12.

Старый native MeshInitialize capture отклонял `flags&3E`, поэтому не доказывал наличие этих authored weighted bytes. Во время рисования mesh `+68/+6C` — лишь optional CPU staging, а не гарантированный источник. Исторический packed combiner повторно увеличивает stored mesh byteSize; actual stride×count и подтверждённый buffer range нужны вместо доверия этому полю. См. [DX materialization](native-pc-dx-materialization.md) и [VertexBuffer layout](native-class-sp-vertex-buffer.md).

Новая общая weighted-capture extraction выполняется отдельно и присутствует в final compile snapshot; её tests/live provenance остаются отдельным доказательством. Текущий observer не разрешает weighted Resolve, не отправляет mesh/instance API, не проверяет bridge skin deformation и не закрывает lifetime всех Skin/Node объектов. Эти границы сохраняются независимо от результата изолированного GPU skin-теста.