# `wxYetiAttackState`

`wxYetiAttackState` (Class ID `292768B3`) — физический потомок [wxCharacterState](wx-character-state.md), selector `3`, native size `3C`, без дополнительных полей. [Реализация](../../../Winx/Code/wxYetiAttackState.cpp) сохраняет собственные update, permission и event hooks. Переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Update, PC slot `30` | `519FB0` | `2EFAE0` |
| Permission, PC slot `34` | `519E50` | `2EFD40` |
| Event, PC slot `3C` | `519EA0` | `2EFBD0` |

Update задаёт `key & F01FFF8F`, делает lookup и при changed handle выполняет release → mode-zero queue с interrupt. Затем сохраняет handle в pending, включая same-handle случай. Control word не сбрасывается. Базовый entry освобождает pending перед виртуальным update; mode-zero queue очищает matching completion records.

Permission при selector, отличном от `A`, разрешает коды `A` и `21` без query. Constructor задаёт selector `3`, но исходная selector-dependent ветвь сохранена. Null pending разрешается без query. Остальные случаи используют consuming completion query. Pending этот hook не освобождает.

Event обрабатывается в следующем порядке:

| Условие для имени | Действие |
| --- | --- |
| Exact `yeti_backspike_attack` | borrowed receiver `owner+24`, если ненулевой: packet `2755,0,0,0,source,0,0,0` |
| Exact `event_shoot` | controller virtual PC slot `38` (PS2 `40`) с аргументом `0` |
| Substring `event_blast_begin` | controller virtual PC slot `3C` (PS2 `44`) с аргументами `1,1` |
| Exact `event_blast_end` | тот же slot с аргументами `0,1` |

Все сравнения case-sensitive. Blast begin использует `strstr`: допускает prefix/suffix и имеет приоритет над end в составной строке. Остальные имена сравниваются целиком. Borrowed имя поступает из `event+1C -> tag+10`. Controller получается по `owner+124 -> entity+140` на PC, `owner+130 -> entity+14C` на PS2. **У controller branches нет null guard**; они требуют действительного внешнего объекта. Исходные имена методов и тип controller остаются неизвестными.

Обязательный [wxYetiAttackStateHost](../../../Winx/Analysis/Host/wxYetiAttackStateHost.h) отделяет borrowed objects, receiver delivery и controller slots. Пустой Copy, Reset и остальные hooks унаследованы; Clone создаёт начальное состояние.

[Проверка](../../../Winx/Tests/wxYetiAttackStateTests.cpp) охватывает changed update, consuming/bypass permission, четыре ветви событий, prefix/suffix и приоритет substring. Полный игровой runtime и реальные borrowed adapters остаются открытыми.
