# `wxDateTalkingState`

`wxDateTalkingState` (Class ID `11110495`) — потомок [wxCharacterState](wx-character-state.md), selector `35` (десятичный), исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxDateTalkingState.cpp) не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `404460` | `3EFBA0` |
| Vtable | `6FA1D8` | `498F00` |
| Вход, PC slot `1C` | `524090` | `3111E0` |
| Выход, PC slot `20` | `524200` | `311050` |
| Обновление, PC slot `30` | `5242A0` | `3110A0` |

Вход вызывает виртуальное обновление и возвращает `true`; предварительного release нет. Обновление задаёт `(key & FF800E09) | E09`, выполняет lookup. При различии handles освобождает старый, запускает новый с `mode=true`, `interrupt=true`, сохраняет после playback. При совпадении запуск пропускается. Control word не изменяется.

Общий выход отправляет сообщение `27ED`, параметр `1D`, source — это состояние, payload — selector и слово владельца `24` по значению. Затем вызывает выход базы, читающий pending после callback. Контракт совпадает с [wxDateIdleState](wx-date-idle-state.md), включая возможность изменения pending при доставке сообщения.

Обязательный [wxDateStateHost](../../../Winx/Analysis/Host/wxDateStateHost.h) подключает внешние объекты и dispatcher. Остальные hooks, Reset и пустой Copy унаследованы; clone имеет начальные флаги и пустые привязки. Деструктор не останавливает playback. [Проверка](../../../Winx/Tests/wxDateStateTests.cpp) охватывает операции состояния и lifecycle; маршрутизация сообщения остаётся внешней зависимостью.
