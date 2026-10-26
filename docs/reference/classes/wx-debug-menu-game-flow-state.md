# wxDebugMenuGameFlowState

`wxDebugMenuGameFlowState` (`38DE5A48`) физически наследует
[wxGameFlowState](wx-game-flow-state.md). В PC и PS2 выделяется `0x3C` байт;
собственного хвоста после базы не обнаружено. PC Clone создаёт новый
derived object и вызывает унаследованный no-op Copy. Новый объект сохраняет
свои constructor defaults, отдельный owned timer и текущую borrowed input
связь базы.

Исходники —
[wxDebugMenuGameFlowState.h](../../../Winx/Code/wxDebugMenuGameFlowState.h) и
[wxDebugMenuGameFlowState.cpp](../../../Winx/Code/wxDebugMenuGameFlowState.cpp).
Внешние input/debug operations обязательны через
[wxDebugMenuGameFlowStateHost](../../../Winx/Analysis/Host/wxDebugMenuGameFlowStateHost.h).
Layout и адреса записаны отдельно для
[PC](../../../Winx/Analysis/PC/wxDebugMenuGameFlowStateAbi.h) и
[PS2](../../../Winx/Analysis/PS2/wxDebugMenuGameFlowStateAbi.h).

## Порядок hooks

Названия отражают смещения слотов PC. На PS2 им соответствуют `28` и `2C`
с учётом заголовка таблицы.

| Hook | Общие операции | Дополнение PC |
| --- | --- | --- |
| `20` | Reset input, повторное чтение input у захваченного owner, mode `7` | Разрешение debug service и вызов consumer `5CCEC0` |
| `24` | Reset input | Разрешение debug service и вызов consumer `5CDB90` |

На PS2 эти hooks завершаются после input operations; PC debug consumers в
них отсутствуют. В hook `20` owner захватывается до Reset, а его `member1C`
читается повторно после Reset. Замена member влияет на mode-вызов; замена
глобального owner не заменяет уже захваченный owner. PC debug service
разрешается после input callbacks, поэтому callback может заменить service
перед его использованием.

Input hooks читают `member1C` singleton owner, тогда как конструктор базы
сохраняет `member18`. Эти связи не объединяются. Остальные известные
empty/true hooks остаются в общей базе.

## Область восстановления

Собственный `HookPlatformForAnalysis` выбирает различия hooks. Обе его
версии используют PC-реконструкцию parent constructor/lifetime; PS2 parent
global defaults этим профилем не воспроизводятся. Host обязан предоставить
borrowed services и реальные consumer effects для своего окружения.
Пустые tokens вызывают отдельный аналитический отказ, а не модель нативных
lazy-constructor/null-fault ветвей.

`WinxDebugDialogGameFlowStateTests` проверяет capture/reload input, изменение
service внутри callbacks и fresh-parent clone. Оригинальное сравнение PC
hooks останавливается на входе настоящего debug consumer и не возобновляет
guest; полный PC factory/RTTI/Clone/delete проверяется отдельно. PS2 hooks
достигают оригинального return с явно заданными input observations и
stack/control ISA seams. Debug consumer bodies, PS2 heap/lifetime,
аппаратная EE семантика и живое меню остаются открытыми.
