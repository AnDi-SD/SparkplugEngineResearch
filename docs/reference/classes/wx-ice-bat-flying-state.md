# `wxIceBatFlyingState`

`wxIceBatFlyingState` (Class ID `CBADEF33`) — потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxIceBatFlyingState.cpp) не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `4031A0` | `3F2DA0` |
| Vtable | `6F8EF8` | `499EA0` |
| Обновление, PC slot `30` | `519B80` | `2EAF40` |

Обновление выполняется только при once-флаге `1D`. Сначала освобождает pending, затем выполняет lookup с неизменённым ключом запроса и сохраняет найденный handle до запуска с `mode=true`, `interrupt=true`. После playback очищает `1D`. Проверки совпадения handles нет: старый handle освобождается и запускается снова, даже если lookup вернул тот же указатель. Control word не изменяется.

Вход и остальные hooks, Reset и пустой Copy унаследованы от базы. Reset снова устанавливает `1D`. Clone создаёт начальное состояние; деструктор не останавливает playback. Обязательный [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h) подключает внешнюю анимацию и владельца. [Проверка](../../../Winx/Tests/wxFishIceBatStateTests.cpp) включает совпадающий handle, повторный update и Reset.
