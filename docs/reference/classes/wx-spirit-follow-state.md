# `wxSpiritFollowState`

`wxSpiritFollowState` (Class ID `F3343612`) — потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`; дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxSpiritFollowState.cpp) не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `403260` | `3F2BA0` |
| Vtable | `6F8F88` | `499E00` |
| Обновление, PC slot `30` | `5A7AF0` | `2EE720` |

Обновление выполняется только при once-флаге `1D`. Освобождает старый pending, затем задаёт `(key & FF9FFFDF) | 50`, выполняет lookup и сохраняет новый handle до запуска с `mode=true`, `interrupt=true`. Только после playback очищает `1D`. Повторный вызов не меняет ключ и не обращается к host; общий Reset снова устанавливает флаг. Control word не изменяется.

Вход и остальные hooks унаследованы от базы. Copy пуст, clone создаёт начальное состояние, деструктор не останавливает playback. Обязательные lookup и consumer-вызовы предоставляет [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [Проверка](../../../Winx/Tests/wxAdditionalCharacterStateTests.cpp) включает флаг, порядок сохранения handle, clone и Copy. Поведение внешней игровой сцены остаётся за host.
