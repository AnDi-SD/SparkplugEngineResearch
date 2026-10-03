# `wxMinotaurMovingState`

`wxMinotaurMovingState` (Class ID `5E63555F`) использует [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `40`. Дополнительный byte `3C` обнуляется конструкторами PC и PS2; padding не получает значения. [Переносимая реализация](../../../Winx/Code/wxMinotaurMovingState.cpp) сохраняет собственные entry, update и permission. Она не претендует на игровой ABI.

| Операция | PC | PS2 |
| --- | --- | --- |
| Вход, PC slot `1C` | `521980` | `3053A0` |
| Обновление, PC slot `30` | `5219C0` | `305130` |
| Permission, PC slot `34` | `521970` | `305410` |

Вход сначала обнуляет byte `3C`, выполняет виртуальный update и вызывает базовый entry. Базовый entry освобождает текущий pending и снова вызывает update. Второе обновление видит изменённые byte, motion и completion records; его нельзя устранять как повторное.

Update получает borrowed control object PC `owner+12C` / PS2 `owner+138` и читает motion float `+4`. Значение меньше `0.05f` (bits `3D4CCCCD`) задаёт mode `0` и обнуляет control word через повторное чтение владельца. Иначе PC снова читает motion из первоначального control pointer; PS2 сохраняет первое значение в `F1`. Значение меньше `0.15f` (bits `3E19999A`) записывает `0.1f` (`3DCCCCCD`) в первоначальный control и задаёт mode `40`. Остальные значения задают mode `50`. Равенство порогу выбирает следующую ветвь. PC unordered comparisons также идут в следующие ветви.

Ключ затем маскируется `F0000070`. Если lookup возвращает текущий pending, update завершается, сохраняя уже выполненные motion writes; completion не проверяется. Если handle отличается и byte `3C` ненулевой, consuming query обновляет byte логическим отрицанием результата. Если byte после этого нулевой, mode `40` устанавливает byte `3C=1` и запускает новый handle с `mode=false`; остальные modes используют `mode=true`. В обеих ветвях старый pending освобождается до playback, `interrupt=true`, а новый pending сохраняется после playback.

Если byte остаётся ненулевым, pending не меняется и control word текущего владельца получает `0.1f`. Запись выполняется после lookup/query; она использует повторно разрешённый control pointer, а не обязательно первоначальный объект. Permission возвращает `byte3C==0` независимо от кода.

Exit и служебные hooks унаследованы. Пустой Copy не переносит byte `3C`; Clone получает constructor zero. Базовый Reset не меняет byte `3C`. Деструктор не останавливает playback.

[wxMinotaurMovingStateHost](../../../Winx/Analysis/Host/wxMinotaurMovingStateHost.h) подключает control object и его raw writes. Lookup, predicate и playback используют обязательный базовый host; выдуманного успешного fallback нет. [Проверка](../../../Winx/Tests/wxMinotaurMovingStateTests.cpp) охватывает оба порога, clamp, consuming query двух completion records, порядок entry, repeated handle, Copy/Clone и Reset. PS2 numeric profile ограничен стабильными нормальными конечными float32 и знаковыми нулями. Special/denormal EE FPU и полный игровой runtime остаются открытыми.
