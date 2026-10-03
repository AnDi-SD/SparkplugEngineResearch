# `wxIceWormHolesState`

`wxIceWormHolesState` (Class ID `1D213B00`) — физический потомок [wxCharacterState](wx-character-state.md), selector `19` (25 в десятичной записи), исходный размер `3C`, дополнительных полей нет. [Реализация](../../../Winx/Code/wxIceWormHolesState.cpp) сохраняет три независимых once-флага и порядок переходов. Переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Вход, PC slot `1C` | `5A7990` | `2ECB30` |
| Выход, PC slot `20` | `5A7A40` | `2EC9D0` |
| Обновление, PC slot `30` | `5A7AF0` | `2EC900` |

При первом входе (`1C=true`) состояние освобождает pending **до** изменения запроса, преобразует key в `(key & FFBFFFDF) | 200050`, выполняет lookup, сохраняет новый pending **до** queue и запускает с `mode=false`, `interrupt=true`. Затем очищает `1C` и возвращает `false`. Повторный вход вызывает consuming completion query, включая null handle. При незавершённой анимации возвращает `false`; иначе отправляет уведомление с флагом `false` и вызывает базовый entry. Базовый entry освобождает pending и вызывает виртуальный update.

Первый выход (`1E=true`) выполняет аналогичный порядок с `(key & FFDFFFDF) | 400050`, mode-zero queue, очисткой `1E` и результатом `false`. При повторном выходе consuming completion query либо оставляет ожидание, либо приводит к уведомлению с флагом `true` и базовому release с результатом `true`.

Update выполняется только при `1D=true`: release → `(key & FF9FFFDF) | 50` → lookup → store → mode-one queue → очистка `1D`. Повторный update пустой. Ни один из трёх hooks не сбрасывает control word. Reset базового класса снова задаёт `1C/1D/1E=true` без stop/fade. Пустой Copy и остальные hooks унаследованы; Clone создаёт начальное состояние.

Уведомление адресовано borrowed receiver `owner+24`, если он ненулевой. Packet содержит `2739, 0, 0, 0, source, 0, flagWord, 0`. **У `flagWord` оригинал задаёт только младший байт** (`0` на entry, `1` на exit); остальные три байта не инициализируются. Их содержимое неизвестно и не объявляется нулём. PC общий builder `435D30` копирует это слово целиком; PS2 строит packet внутри hooks.

Обязательный [wxIceWormHolesStateHost](../../../Winx/Analysis/Host/wxIceWormHolesStateHost.h) передаёт receiver и известный логический флаг; это наш адаптер, а не утверждение о полном 32-битном payload. [Проверка](../../../Winx/Tests/wxIceWormHolesStateTests.cpp) охватывает цепочку вход → completion → update → выход → completion, consuming/reset completion records и store перед queue. Borrowed delivery, реальный игровой runtime и содержимое padding остаются вне восстановленного контракта.
