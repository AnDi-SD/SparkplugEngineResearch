# `wxDateReactionState`

`wxDateReactionState` (Class ID `623B778B`) — потомок [wxCharacterState](wx-character-state.md), selector `36` (десятичный), исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxDateReactionState.cpp) не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `404400` | `3EFCA0` |
| Vtable | `6FA190` | `498F50` |
| Вход, PC slot `1C` | `524190` | `310E40` |
| Выход, PC slot `20` | `524200` | `310DE0` |
| Permission, PC slot `34` | `5203D0` | `310F80` |

Вход задаёт `(key & FF800009) | 9` и выполняет lookup. При null результате возвращает `true`, сохранив прежний pending и playback. При ненулевом результате освобождает старый handle, запускает найденный с `mode=false`, `interrupt=true`, сохраняет после playback и возвращает `true`. Совпадение handles не пропускает release/play. Виртуальное обновление из входа не вызывается; само обновление унаследовано и пусто. Control word не изменяется.

Permission возвращает `true` при null pending, без вызова consumer; иначе выполняет consuming completion query. Выход совпадает с [wxDateIdleState](wx-date-idle-state.md): сообщение `27ED`, параметр `1D`, source — это состояние, payload — selector и слово владельца `24` по значению, затем базовый release с актуальным pending после callback.

Для выхода нужен обязательный [wxDateStateHost](../../../Winx/Analysis/Host/wxDateStateHost.h). Другие hooks, Reset и пустой Copy унаследованы. Clone имеет начальные flags и пустые привязки; деструктор не останавливает playback. [Проверка](../../../Winx/Tests/wxDateStateTests.cpp) включает null lookup, повторный запуск того же handle и permission; полная игровая сцена остаётся внешней зависимостью.
