# DebugMenu, DialogWindow и LoadSave: короткие active hooks

После общего lifecycle изучены собственные короткие методы трёх состояний.
10 paired PC/PS2 cases прошли за1,772 с. На PC часть методов выполнена целиком,
часть остановлена на входе в следующий игровой consumer; на PS2 границы
установлены независимо. Чужой игровой метод не заменяется успешной заглушкой.

Индексы `v8/v9/v12` ниже считаются после двух PS2 ABI words: это PC byte slots
`20/24/30`, PS2 `28/2C/38`. Их исходные source method names пока не установлены.
Связи подтверждены настоящими vtable из
[первого прохода](native-game-flow-family-construction-2026-09-10.md).

## Общий ввод и различия платформ

Borrowed `wxUserInput` owner предоставляет object в `+1C`. На нём проверяется
порядок virtual calls: PC slot `20` без дополнительных аргументов, затем
при необходимости slot `1C` с integer7; PS2 соответствующие slots `28/24`.
В probe наблюдатели названы `reset-observation/mode-observation`; это
аналитические метки, не доказанные original names или реализация этих методов.
Их внутренние эффекты не моделируются; регистрируются receiver/argument/order.

| Состояние, метод | PC | PS2 |
|---|---|---|
| DebugMenu v8 | `5DD980`: input slot20 → slot1C(7) → `wxDebugManager::5CCEC0` | `326A20`: input slot28 → slot24(7), других consumer calls нет |
| DebugMenu v9 | `5DD9D0`: input slot20 → `wxDebugManager::5CDB90` | `3269D0`: только input slot28 |
| DialogWindow v9 | `5DB290`: zero150 dwords по window `+14` | `327DE0`: zero600 bytes по window `+14` через `4076A8` |
| DialogWindow v12 | `5DB2C0`: вызов window `5C98B0`, затем bool true | `327D80`: вызов window `36C400`, затем bool true |
| LoadSave v9 | `5DB140`: только input slot20 | `344270`: input slot28; дополнительное условие manager state |
| LoadSave v12 | `5DB160`: input slot20 → slot1C(7), bool true | `3441C0`: input slot28 → slot24(7) → load/save manager `21D2F0`, затем bool true |

PC `wxDebugManager` identity проверена по factory `5CC160`/record`767BB0`,
window — `wxDialogWindow` factory`5CA1B0`/record`767B50`.
PS2 window независимо найден по factory`36CAD0`/record`4C4AC0`,
`wxLoadSavePS2Manager` — factory`21F470`/record`4BA310`.
Их полное поведение этим проходом не исследовано; имя класса не означает,
что краткий вызов уже доказывает все его эффекты.

## Точные локальные эффекты

PC DialogWindow v9 исполняет `rep stosd`: диапазон `[window+14,window+26C)`
обнуляется, всего `258` hex /600 decimal bytes. Проверены заполненные `A5`
данные и неизменённые guards до/после диапазона. PS2 original instructions
независимо формируют `(window+14,0,600)`; probe доходит до входа memset helper,
не выдавая эту границу за завершённую очистку полной PS2-функцией.

PS2 LoadSave v9 читает `wxLoadSavePS2Manager +6A4`. Только при точном значении4
обращается к `wxGameFlowController::33A890` с аргументами `(54,1)`;
остальные значения завершают метод после input callback. Проверены
0,3,4,5,`FFFFFFFF`: четыре original returns и одна граница consumer с
точными аргументами после MIPS delay slot. Что означает transition code54
и как controller обрабатывает запрос, этим наблюдением не подменяется.
Соответствующий PC метод не читает этот manager/state.

PS2 LoadSave v12 вызывает настоящий entry manager`21D2F0`: consumer
начинает dispatch по unsigned state `+6A4<7`, но его большая машина
состояний/PS2 storage API остаются отдельным исследованием. Probe намеренно
заканчивается перед этим consumer. PC DebugMenu и обе window v12 аналогично
проверяют вызов/receiver и останавливаются до неизвестного body.

Bool true после таких consumer подтверждён original instructions;
сам consumer и последующий return в prefix cases не исполнялись.
PC LoadSave v12 реально вернулся с AL=1. Пустые методы и inherited true hook
подтверждены vtables и прямыми leaf bodies, без повторного прогона всей семьи.

## Проверка и оценка

Каждая platform/case использует fresh guest. PC — micro100000 instructions/2 с;
PS2 — ordinary integer1000/100ms. SD/LD-only методы получают реальный стек;
там, где prologue содержит SQ, entry перенесён за него с явно заданными
исходными регистрами, stop расположен до LQ epilogue или игрового consumer.
Заданные globals исключают lazy startup; его успешность не утверждается.

Оценки после этого блока: DebugMenu PC60/PS255, DialogWindow PC65/PS260,
LoadSave PC70/PS260. Классы остаются частично изученными: own hooks установлены
значительно лучше, но inherited base/resource lifetime, lazy startup,
полные consumer effects и исходные имена открыты. Оценки не начисляются
попутно вызванным managers только за registration или остановку на их входе.

Evidence: `local-data/results/native-cycle-20260910-1900/game-flow-small/`:
`paired-run1.json`, сохранённый `probe_game_flow_small_hooks.py`,
`windows/capture.json`, `windows2/capture.json`. Window labels в raw captures
служат навигации; смысл методов ограничен описанными выше проверенными фактами.
