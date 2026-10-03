# `wxMikaelWaitingState`

`wxMikaelWaitingState` (Class ID `79DC43A8`) — физический потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxMikaelWaitingState.cpp) задаёт собственные entry/update hooks. Игровой ABI описывается отдельно от размера переносимого объекта.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Вход, PC slot `1C` | `5A8190` | `314A80` |
| Обновление, PC slot `30` | `5A82A0` | `314920` |

Вход сначала вызывает виртуальное обновление, затем очищает control word PC `*(owner+12C)+4` / PS2 `*(owner+138)+4`, после чего вызывает базовый вход. Базовый вход освобождает текущий pending и снова вызывает виртуальное обновление. Таким образом, lookup выполняется дважды, а handle первого обновления может быть освобождён и снова запущен. Второй вызов нельзя устранять как повторный.

Обновление преобразует ключ запроса в `(key & F0800000) | 800000`, сохраняя верхний nibble и задавая subfield `23` равным `1`. Выполняет lookup. Совпадение нового handle с pending пропускает release/playback. При различии освобождает старый, запускает новый с `mode=true`, `interrupt=true` и сохраняет его после playback. Сам update не очищает control word.

Остальные hooks, пустой Copy и Reset унаследованы. Clone создаёт начальное состояние с selector `0`; деструктор не останавливает playback. Внешние lookup, owner predicate и animation controller требуют обязательного [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [Проверка](../../../Winx/Tests/wxMikaelWaitingStateTests.cpp) сохраняет порядок двух обновлений, release и control clear. Оригинальные имена виртуальных hooks неизвестны; полный игровой runtime остаётся внешней зависимостью.
