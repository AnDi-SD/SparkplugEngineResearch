# `wxDroidMovingState`

`wxDroidMovingState` (Class ID `1CC66F37`) — потомок [wxCharacterState](wx-character-state.md) с selector `0`, исходным размером `3C` и без дополнительных полей. [Общая реализация](../../../Winx/Code/wxDroidMovingState.cpp) использует переносимый layout, который не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `404340` | `3EFEA0` |
| Vtable | `6FA0F8` | `498FF0` |
| Обновление, PC slot `30` | `523FC0` | `312960` |

Обновление читает float из control word `4` владельца. При значении `<= 0.1f` применяет к ключу `FFFFFF8F`, иначе задаёт `(key & FFFFFFDF) | 50`. После этого применяет `F007FFFF`, выполняет lookup и сравнивает найденный handle с pending. При совпадении playback пропускается. При различии сначала освобождает старый handle, запускает новый с `mode=true`, `interrupt=true`, затем сохраняет его. Control word не обнуляется.

Равенство порогу относится к неподвижной ветви. PC unordered относится к ветви движения. Для PS2 подтверждены нормальные конечные float и знаковые нули. Специальные и денормальные значения остаются отдельной границей переноса.

Другие hooks унаследованы от базы; PS2 имеет эквивалентные собственные leaves `true/false` для slots `34/38`. Вход базы освобождает pending и вызывает виртуальное обновление. Copy пуст, clone получает начальные привязки и флаги; деструктор не останавливает playback. Доступ к motion предоставляет обязательный [wxCharacterMotionStateHost](../../../Winx/Analysis/Host/wxCharacterMotionStateHost.h). [Проверка](../../../Winx/Tests/wxAdditionalCharacterStateTests.cpp) охватывает операции состояния и lifecycle; полноценная сцена требует внешних игровых объектов.
