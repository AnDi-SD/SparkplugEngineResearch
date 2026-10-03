# `wxIceGargoyleMovingState`

`wxIceGargoyleMovingState` (Class ID `53952BB6`) — физический потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, дополнительных полей нет. [Реализация](../../../Winx/Code/wxIceGargoyleMovingState.cpp) сохраняет собственные PC-ветви; область соответствия PS2 для сравнения float ограничена нормальными конечными значениями и знаковыми нулями. Переносимый объект не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `4030E0` | `3F2FA0` |
| Vtable | `6F8E68` | `499F40` |
| Выход, PC slot `20` | `5198E0` | `2EEC40` |
| Обновление, PC slot `30` | `519830` | `2EEDE0` |

Обновление задаёт младшую часть запроса `(key & FFFFFFF1) | 1`. Читает packed key владельца по PC `owner+140` / PS2 `owner+14C`. Если `(action & 7F80) == 800`, очищает mode маской `FFFFFF8F` и не читает float движения. Иначе читает float `4` по цепочке PC `owner+124 -> object+130`, PS2 `owner+130 -> object+13C`. При значении `<= 0` очищает mode той же маской; иначе задаёт `(key & FFFFFFDF) | 50`. PC unordered также выбирает `50`. После маски `F01FFFFF` выполняет lookup. При изменении handle освобождает старый, запускает новый с `mode=true`, `interrupt=true` и затем сохраняет его. Совпавший handle пропускает release/playback; control не очищается.

Выход при action, отличном от `800`, вызывает базовый выход: release и `true`. При action `800` и установленном once-флаге `1E` очищает младшие семь bits, применяет `(key & F05FFFFF) | 400000`, освобождает pending, выполняет lookup и запускает с `mode=false`, `interrupt=true`. После playback сохраняет handle, очищает `1E`, сбрасывает control word и возвращает `false`. При повторном выходе вызывает consuming completion query, включая null handle. При завершении выполняет базовый выход без сброса control; иначе сбрасывает control и возвращает `false`.

Внешние поля и playback доступны через обязательный [host-адаптер](../../../Winx/Analysis/Host/wxIceGargoyleMovingStateHost.h). Это наш интерфейс; оригинальные имена action и внешних типов неизвестны. Copy пустой, Clone создаёт начальное состояние, Reset унаследован, деструктор не останавливает playback. [Проверка](../../../Winx/Tests/wxIceGargoyleMovingStateTests.cpp) охватывает переход update → первый выход → ожидание → завершение, порядок callbacks и исходные флаги. Полная сцена и EE FPU special/denormal semantics остаются за пределами установленного поведения.
