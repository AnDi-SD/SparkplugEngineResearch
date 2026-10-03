# `wxOpenGateState`

`wxOpenGateState` (Class ID `7C546AAD`) — физический потомок [wxCharacterState](wx-character-state.md), selector `1B` (27 в десятичной записи), native size `3C`, без дополнительных полей. [Реализация](../../../Winx/Code/wxOpenGateState.cpp) сохраняет собственные update, permission и event hooks. Переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Update, PC slot `30` | `5A7800` | `2D03C0` |
| Permission, PC slot `34` | `5A77C0` | `2D04E0` |
| Event, PC slot `3C` | `5A7770` | `2D0550` |

Update задаёт `(key & FF878A80) | A80`, делает lookup и при changed handle выполняет mode-zero queue с interrupt → store pending, **без release старого handle**. Mode-zero queue сбрасывает matching completion records. Затем, включая same-handle случай, очищает action control word. Базовый entry предварительно освобождает pending и вызывает этот update.

Permission игнорирует код перехода и всегда вызывает consuming completion query, включая null handle. При успехе отправляет filtered notification `27DE` с filter `12` и нулевыми payload словами; возвращает результат query. Once-флага нет: каждый успешный вызов отправляет сообщение снова. Completion records потребляются, pending не освобождается этим hook.

Event читает borrowed имя из `event+1C -> tag+10` и сравнивает целиком, case-sensitive, с `event_spin`. При совпадении читает borrowed `owner+124` (PS2 `+130`) и отправляет `2731`, filter `6`, payload `(entity,0)`, включая null entity. При несовпадении entity не читается.

PC builder `40EC00` (PS2 `1007A0`) формирует packet `code,0,0,filter,source,0,payload0,payload1` и передаёт его внешней службе сообщений. Значение filter и доставка не переименовываются в предполагаемые игровые понятия. Обязательный [wxOpenGateStateHost](../../../Winx/Analysis/Host/wxOpenGateStateHost.h) явно отделяет эти операции.

Пустой Copy, Reset, остальные hooks унаследованы; Clone создаёт начальное состояние. [Проверка](../../../Winx/Tests/wxOpenGateStateTests.cpp) охватывает entry, completion consumption, repeated/null permission notifications и event с несовпадающим suffix. Внешняя доставка и полный игровой runtime остаются открытыми.
