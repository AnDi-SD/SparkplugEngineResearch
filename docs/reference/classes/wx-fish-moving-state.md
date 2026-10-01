# `wxFishMovingState`

`wxFishMovingState` (Class ID `3A7A13AB`) — потомок [wxCharacterState](wx-character-state.md) с selector `0`, исходным размером `3C` и без дополнительных полей. [Переносимая реализация](../../../Winx/Code/wxFishMovingState.cpp) не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `403740` | `3F1EA0` |
| Vtable | `6F9598` | `4999F0` |
| Обновление, PC slot `30` | `51F7F0` | `30A650` |
| Permission, PC slot `34` | `51F7A0` | `30A7D0` |

Обновление при выключенном `1D` ничего не меняет. При включённом флаге задаёт `(key & FF800050) | 50`, выполняет lookup и сравнивает handle с pending. При различии запускает новый handle с `interrupt=true`: `mode=true`, если `(key & 0F800000) == 0`, иначе `mode=false`. После playback сохраняет pending. Старый handle не освобождается, флаг `1D` не очищается; при совпадении handle пропускается только playback. Control word не изменяется.

Permission возвращает `true`, если selector равен нулю либо pending пуст. В остальных случаях вызывает consuming completion query. Фабрика создаёт selector `0`, поэтому обычный экземпляр не потребляет completion-записи этим hook. Slot `38` возвращает `false`.

Остальные hooks, Reset и пустой Copy унаследованы. Clone имеет начальные flags и пустые привязки; деструктор не останавливает анимацию. Внешние объекты предоставляет обязательный [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [Проверка](../../../Winx/Tests/wxFishIceBatStateTests.cpp) включает оба playback mode и повторные обновления; живая игровая сцена остаётся внешней зависимостью.
