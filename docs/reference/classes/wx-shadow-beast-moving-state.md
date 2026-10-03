# `wxShadowBeastMovingState`

`wxShadowBeastMovingState` (Class ID `361D2C0E`) — физический потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, дополнительных полей нет. [Реализация](../../../Winx/Code/wxShadowBeastMovingState.cpp) сохраняет собственные PC entry/update hooks.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Вход, PC slot `1C` | `523550` | `3103D0` |
| Обновление, PC slot `30` | `523590` | `3101D0` |

Вход сначала вызывает виртуальное обновление, затем базовый вход. Базовый вход освобождает pending и вызывает update повторно; два lookup и возможное повторное playback сохраняются.

Update читает control float `4` по PC `owner+12C` / PS2 `owner+138`. При значении `< 0.2f` очищает mode маской `FFFFFF8F`, затем задаёт `(key & F0FFFFFF) | 800000`. PC между этими записями читает control byte `1D`; его значение не используется в результате. Иначе PC повторно читает float `4`: при значении `< 0.5f` задаёт `(key & F07FFFDF) | 50`, иначе `(key & F0FFFFDF) | 800050`. PC unordered выбирает последнюю ветвь. После всех ветвей применяет `FF800070` и выполняет lookup. При изменении handle выполняет release → mode-one queue → store; одинаковый handle пропускает playback. Control word не сбрасывается.

PS2 удерживает первое значение motion в FPU-регистре для второго сравнения и не читает byte `1D`. Переносимый компонент сохраняет порядок чтений PC; соответствие результата PS2 ограничено стабильным normal finite float32 и знаковыми нулями. Special/denormal EE FPU semantics пока неизвестны.

Borrowed чтения вынесены в обязательный [wxShadowBeastMovingStateHost](../../../Winx/Analysis/Host/wxShadowBeastMovingStateHost.h), playback использует общий host. Пустой Copy, Reset и остальные hooks унаследованы; Clone создаёт начальное состояние, деструктор не останавливает playback. [Проверка](../../../Winx/Tests/wxShadowBeastMovingStateTests.cpp) охватывает оба порога, промежуточный ключ при чтении byte и повторное обновление entry. Полная сцена остаётся внешней зависимостью.
