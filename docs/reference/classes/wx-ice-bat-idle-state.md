# `wxIceBatIdleState`

`wxIceBatIdleState` (Class ID `338E72B1`) — потомок [wxCharacterState](wx-character-state.md) с selector `0`. Исходный размер `3C`; дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxIceBatIdleState.cpp) сохраняет порядок вызовов PC/PS2, не задавая игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `403140` | `3F2EA0` |
| Vtable | `6F8EB0` | `499EF0` |
| Выход, PC slot `20` | `519A90` | `2EB080` |
| Обновление, PC slot `30` | `519A30` | `2EB160` |

Обновление выполняется только при флаге `1D`. Сначала освобождает pending, затем применяет к ключу `FF9FFFFF`, выполняет lookup и сохраняет новый handle **до** запуска с `mode=true`, `interrupt=true`. После запуска очищает `1D`. Повторное обновление без Reset ничего не меняет.

Первый выход при `1E` освобождает pending до изменения ключа, задаёт `(key & FFDFFFFF) | 00400000`, выполняет lookup, сохраняет handle до запуска с `mode=false`, `interrupt=true`, очищает `1E` и возвращает `false`. Повторный выход запрашивает завершение с потреблением записей, включая случай null. Пока анимация не завершена, возвращает `false`; после завершения вызывает общий выход базы, освобождающий pending и возвращающий `true`. Control word не изменяется.

Остальные hooks, Reset и пустой Copy унаследованы. Clone имеет начальные флаги и пустые привязки; деструктор не останавливает анимацию. Обязательные внешние вызовы задаёт [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [Проверка](../../../Winx/Tests/wxAdditionalCharacterStateTests.cpp) включает повторные операции, записи completion, RTTI и lifecycle.
