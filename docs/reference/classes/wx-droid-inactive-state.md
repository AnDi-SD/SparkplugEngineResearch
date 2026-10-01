# `wxDroidInactiveState`

`wxDroidInactiveState` (Class ID `31FC4D4C`) — потомок `wxCharacterState` с selector `2`. [Общая реализация](../../../Winx/Code/wxDroidInactiveState.cpp) содержит собственные hooks PC/PS2. Исходный размер — `3C`, дополнительных полей нет. Переносимый layout не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `4042E0` | `3EFFA0` |
| Vtable | `6FA0B0` | `499040` |
| Выход, PC slot `20` | `523EC0` | `3125F0` |
| Обновление, PC slot `30` | `523E50` | `312760` |

Первый выход при once-флаге `1E` меняет ключ на `(key & F0478003) | 00400003`. После lookup освобождает старый handle, запускает новый с `mode=false`, `interrupt=true`, сохраняет handle и очищает `1E`. Затем обнуляет control word `4` и возвращает `false`. Флаг `1F` не участвует в этой проверке.

Повторный выход запрашивает завершение с потреблением completion-записей. При `false` обнуляет control word `4` и возвращает `false`. При `true` вызывает общий выход [wxCharacterState](wx-character-state.md): освобождает pending handle и даёт `true`, без обнуления control word.

Обновление задаёт `(key & F0078003) | 3`, выполняет lookup и при изменении handle освобождает старый, запускает новый с `mode=true`, `interrupt=true`, сохраняет после playback. Одинаковый handle пропускает release/play. Само обновление control word не очищает. Вход базы освобождает handle и вызывает это виртуальное обновление.

Slots `34/38` используют общие `true/false` hooks на PC и собственные эквивалентные leaves на PS2. Copy пуст и унаследован от `spBaseObject`; clone получает начальные flags и пустые привязки. Деструктор не останавливает анимацию. Внешние игровые зависимости подключаются через обязательный [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [C++ проверка](../../../Winx/Tests/wxDroidMosquitoStateTests.cpp) охватывает оба этапа выхода, маски, callbacks, Copy и clone; полная игровая сцена остаётся внешней зависимостью.
