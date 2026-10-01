# `wxFlyingState`

`wxFlyingState` (Class ID `7323811A`) — потомок [wxCharacterState](wx-character-state.md) с selector `20`. Исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxFlyingState.cpp) не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `402D80` | `3F38A0` |
| Vtable | `6F8B40` | `49A210` |
| Обновление, PC slot `30` | `518520` | `2CCAE0` |
| Вход — переход в базу | `5184E0` | `2CCAD0` |
| Выход — переход в базу | `5184F0` | `2CCAC0` |

Обновление задаёт `(key & F007FFF1) | 1`, выполняет lookup и сравнивает найденный handle с pending. При совпадении playback пропускается. При различии освобождает старый handle, запускает новый с `mode=true`, `interrupt=true`, затем сохраняет его. Control word не обнуляется. Вход и выход переходят непосредственно в общие hooks базы; вход после release вызывает виртуальное обновление.

Остальные hooks, Reset и пустой Copy унаследованы. Clone имеет начальные flags и пустые привязки. Деструктор не останавливает анимацию. Для внешних вызовов нужен обязательный [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [Проверка](../../../Winx/Tests/wxFlyingHoistStateTests.cpp) включает совпадающий handle, callbacks, factory/RTTI и lifecycle; игровая сцена остаётся внешней зависимостью.
