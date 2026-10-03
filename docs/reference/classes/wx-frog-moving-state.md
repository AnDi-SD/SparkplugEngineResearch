# `wxFrogMovingState`

`wxFrogMovingState` (Class ID `50640A47`) — физический потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, дополнительных полей нет. [Реализация](../../../Winx/Code/wxFrogMovingState.cpp) сохраняет update и event hooks; переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Обновление, PC slot `30` | `520D50` | `2F4730` |
| Событие, PC slot `3C` | `520CE0` | `2F4880` |

Обновление читает float `4` внешнего control-объекта: PC `owner+12C`, PS2 `owner+138`. При значении `< 0.1f` очищает animation mode маской `FFFFFF8F` и обнуляет тот же control word. Иначе задаёт `(key & FFFFFFDF) | 50`. Equality принадлежит движению, как и PC unordered. После маски `F007FFFF` выполняет lookup. Если handle отличается от pending, запускает новый с `mode=true`, `interrupt=true` и сохраняет после playback; старый pending в собственном update не освобождается. Совпадение handles пропускает запуск. Базовый entry освобождает старый handle перед вызовом update.

Событие получает borrowed строку по `event+1C -> tag+10`. Полное регистрозависимое совпадение с `event_hop_end` вызывает чтение `owner+24`. Если receiver ненулевой, передаёт ему packet из восьми 32-битных слов: `27A4, 0, 0, 0, source, 0, name, 0`. `source` — текущее состояние, `name` — исходный указатель из tag. Иное имя не читает receiver. Null receiver пропускает dispatch.

Чтение motion доступно через общий `wxCharacterMotionStateHost`; для события нужен [wxFrogMovingStateHost](../../../Winx/Analysis/Host/wxFrogMovingStateHost.h). Эти интерфейсы — наши адаптеры, без успешной реализации по умолчанию. Остальные hooks, Copy и Reset унаследованы; Clone создаёт начальное состояние. [Проверка](../../../Winx/Tests/wxFrogMovingStateTests.cpp) охватывает motion, порядок queue/store, точное совпадение события и null receiver. Для PS2 сравнение float установлено в области normal finite float32 и знаковых нулей; EE special/denormal semantics и полный runtime сцены остаются открытыми.
