# wxLoadSaveGameFlowState

`wxLoadSaveGameFlowState` (`612C1BB8`) физически наследует
[wxGameFlowState](wx-game-flow-state.md). PC и PS2 выделяют `0x3C` байт;
собственных полей после базы не обнаружено. PC factory вызывает конструктор
базы и заменяет виртуальную таблицу. Его Clone создаёт новый derived object,
регистрирует пару в общем clone manager и вызывает унаследованный no-op Copy.
Коды состояния, owned timer и borrowed input при этом остаются результатом
нового конструктора.

Реализация —
[wxLoadSaveGameFlowState.h](../../../Winx/Code/wxLoadSaveGameFlowState.h) и
[wxLoadSaveGameFlowState.cpp](../../../Winx/Code/wxLoadSaveGameFlowState.cpp).
Внешние операции input/load-save/controller обязательны через
[wxLoadSaveGameFlowStateHost](../../../Winx/Analysis/Host/wxLoadSaveGameFlowStateHost.h).
Нативные адреса и layout описаны для
[PC](../../../Winx/Analysis/PC/wxLoadSaveGameFlowStateAbi.h) и
[PS2](../../../Winx/Analysis/PS2/wxLoadSaveGameFlowStateAbi.h).

## Собственные hooks

Имена `FlowSlot24ForAnalysis` и `FlowSlot30ForAnalysis` обозначают смещения
слотов PC, а не восстановленные имена методов. На PS2 им соответствуют
смещения `2C` и `38` с учётом заголовка таблицы.

| Hook | PC | PS2 |
| --- | --- | --- |
| `24` | Reset текущего input | Reset input; чтение состояния load-save manager по `+6A4`; только при значении `4` вызов controller с параметрами `36,1` |
| `30` | Reset input, mode `7`, результат `true` | Reset input, mode `7`, вызов load-save manager, затем литеральный `true` |

Оба hook берут input из `member1C` внешнего singleton owner. Это отдельная
связь от `member18`, которую конструктор базы сохраняет в своём поле `30`.
Hook `30` удерживает owner через Reset и повторно читает его `member1C`
перед установкой mode. Поэтому замена input внутри Reset влияет на второй
вызов; замена глобального owner не меняет уже захваченный owner. На PS2
состояние load-save manager читается после Reset, а manager для hook `30`
разрешается после установки mode.

## Область восстановления

Собственный `HookPlatformForAnalysis` выбирает различия hooks. Конструктор
и lifetime обоих аналитических профилей используют восстановленную PC-базу;
это не перенос дополнительных PS2 global defaults в конструктор. Разрешение
singleton, реализация input, работа load-save manager и controller остаются
внешними зависимостями. Host не предоставляет для них успешных заглушек.
Пустой borrowed token вызывает собственный отказ аналитического API; нативное
поведение при недоступных зависимостях так не моделируется.

Проверка — `WinxLoadSaveGameFlowStateTests`. Сравнение PC включает исходные
factory/RTTI/Clone/delete и завершённые hooks с объявленными input callbacks.
Для PS2 ветви без game consumer завершаются в оригинальном return; остальные
заканчиваются на входе настоящего consumer, после чего guest отбрасывается.
Тела load-save/controller, PS2 constructor/heap/teardown, аппаратная EE
семантика и игровой UI этим сравнением не квалифицированы.
