# DebugMenu, DialogWindow и LoadSave: короткие active hooks

После общего lifecycle изучены собственные короткие методы трёх состояний.
10 paired PC/PS2 cases прошли за1,772 с. На PC часть методов выполнена целиком,
часть остановлена на входе в следующий игровой consumer; на PS2 границы
установлены независимо. Чужой игровой метод не заменяется успешной заглушкой.

## Общий ввод и различия платформ

| Состояние, метод | PC | PS2 |
| --- | --- | --- |
| DebugMenu v8 | `5DD980`: input slot20 → slot1C(7) → `wxDebugManager::5CCEC0` | `326A20`: input slot28 → slot24(7), других consumer calls нет |
| DebugMenu v9 | `5DD9D0`: input slot20 → `wxDebugManager::5CDB90` | `3269D0`: только input slot28 |
| DialogWindow v9 | `5DB290`: zero150 dwords по window `+14` | `327DE0`: zero600 bytes по window `+14` через `4076A8` |
| DialogWindow v12 | `5DB2C0`: вызов window `5C98B0`, затем bool true | `327D80`: вызов window `36C400`, затем bool true |
| LoadSave v9 | `5DB140`: только input slot20 | `344270`: input slot28; дополнительное условие manager state |
| LoadSave v12 | `5DB160`: input slot20 → slot1C(7), bool true | `3441C0`: input slot28 → slot24(7) → load/save manager `21D2F0`, затем bool true |

## Точные локальные эффекты

Bool true после таких consumer подтверждён original instructions;
сам consumer и последующий return в prefix cases не исполнялись.
PC LoadSave v12 реально вернулся с AL=1. Пустые методы и inherited true hook
подтверждены vtables и прямыми leaf bodies, без повторного прогона всей семьи.

Оценки после этого блока: DebugMenu PC60/PS255, DialogWindow PC65/PS260,
LoadSave PC70/PS260. Классы остаются частично изученными: own hooks установлены
значительно лучше, но inherited base/resource lifetime, lazy startup,
полные consumer effects и исходные имена открыты. Оценки не начисляются
попутно вызванным managers только за registration или остановку на их входе.
