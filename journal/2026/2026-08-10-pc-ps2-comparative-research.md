# 2026-08-10 — расширенный PC/PS2-корпус и GUI SMO

## Вопрос

Какие выводы раннего исследования сохраняются после сравнения независимых PC/PS2 rigs, большого уровня Gardenia01 и GUI-ресурсов?

## Корпус и commit инструментов

Сводные входы перечислены в локальных документах `local-data/Исследование от 10.08.2026`. Использовались Bloom, Bird, Butterfly, Fish, Kikko, Gardenia01, menu/loading resources, PC `WinxClub.exe` и PS2 `SLES_532.19`. На момент переноса результатов `SmoViewer` находился на `91c3b9e`.

Контрольные SHA-256 из документа перепроверены: `bloom_ball.smo` — `8648CFFD906B0BCFA7455B2FBDC2112EB76A939165E17885EAFB04EFC3D27291`, `menu.smo` — `1AFD3D65A4F974BD352F8A94F77697339668A3C13C1E314DDF0BA6188573E844`. У `WinxClub.exe` в локальном corpus есть три одинаковых по размеру варианта; указанный в документе hash `D8D0AD112D46F7229C7227B2D15338195D54957AB408E75C1E566895EF0988C0` соответствует `local-data/Winx Club/WinxClub.exe`, а не `pc-pristine/WinxClub.exe`. Это различие обязательно учитывать при воспроизведении offsets.

## Наблюдение

- FFPS serializer version `0x26` и общий object-graph слой подтверждаются на PC и PS2; platform mask: common `0x01`, PC `0x02`, PS2 `0x08`.
- PC и PS2 `E1` — разные platform-specific mesh layouts. PS2-вариант содержит DMA/VIF stream; `E2` хранит bounding box.
- 16-slot PC и 64-slot PS2 skin palettes подтверждены независимыми rigs. Kikko даёт прямую проверку body palette `16 + 9` на PC против 20 уникальных bones в одной PS2 palette.
- Vertex weights и palette indices находятся в PC vertex records `0x097E`/`0x197E`; logical skeleton hierarchy задаётся `esfNodeChild`.
- `menu.smo` является GUI scene. Подтверждены class ID `spTextNode`, `spTextRenderable`, `spFont` и diffuse-only layout `0x0100` для button states.
- Serializer знает 16/32-bit index buffers; общий hard limit 65 535 не подтверждён.
- PCK index проверен на `BLOOM.PCK` (18 entries) и `ST1.PCK` (247 entries): сохранённый byte offset равен `sectorOffset * 0x800`, а `offset + size` находится в границах архива. Проверенный QuickBMS-скрипт перенесён в `research/winx_sparkplug_pck.bms`.

## Вывод

Основной Core должен оставаться источником истины для FFPS/object graph. Экспорт и импорт следует строить отдельными слоями, сохраняя исходные mesh boundaries и object mapping. Формат обмена и исходный DCC пока не фиксируются: 3ds Max — сильная, но всё ещё косвенная гипотеза.

После переноса изменений strict regression scan на `local-data/pc-pristine/Media` прошёл 416/416 SMO, 177 369 object-directory entries и 22 012 mesh (140 269 assertions). Отдельная инспекция `menu.smo` декодировала 41/41 mesh и назвала по реестру все 30 GUI-объектов трёх новых классов.

## Что не подтвердилось

- Bloom недостаточно для общего вывода о platform partitioning; независимую поддержку дал Kikko, но алгоритм exporter'а ещё не восстановлен.
- Число 65 535 не является установленным лимитом mesh/engine.
- `Kikko_alfea.smo` не является прямой PS2-парой `Kikko.smo`.

## Следующий эксперимент

Сделать byte-identical round-trip одного `spMeshData`, затем controlled replacement без изменения object graph. Отдельно декодировать PS2 DMA/VIF payload и проверить GUI state/text semantics.
