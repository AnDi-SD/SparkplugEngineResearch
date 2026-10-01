# `wxShadowBeastHurtState`

`wxShadowBeastHurtState` (Class ID `0C87213F`) — потомок `wxCharacterState` с selector `10`. [Общая реализация](../../../Winx/Code/wxShadowBeastHurtState.cpp) сохраняет собственные hooks PC/PS2. Исходный размер — `3C` на обеих платформах, собственных полей нет. Переносимый layout не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `404160` | `3F03A0` |
| Vtable | `6F9F88` | `499180` |
| Вход, PC slot `1C` | `5236D0` | `30FD20` |
| Обновление, PC slot `30` | `523690` | `30FD10` |
| Проверка перехода, PC slot `34` | `523770` | `30FE90` |

Вход меняет ключ на `(key & F0008000) | 8000`, выполняет lookup и playback с `mode=false`, `interrupt=true` без освобождения прежнего pending handle. После playback сохраняет новый handle и вызывает виртуальное обновление `30`, обнуляющее control word `4`. Вход возвращает `true`, once-флаги не меняются. В отличие от [wxFrogHurtState](wx-frog-hurt-state.md), byte `1A` control не очищается.

Переход `34` всегда вызывает completion consumer с текущим handle и `consume=true`, включая пустой handle. Target не используется. Slot `38` даёт `false`: общий PC hook и собственный эквивалентный PS2 leaf. Выход, reset и остальные hooks используют [wxCharacterState](wx-character-state.md). Copy пуст, clone сохраняет значения свежего конструктора и свой тип; деструктор не останавливает анимацию.

Внешние lookup, playback, completion и control подключаются через обязательный [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [C++ проверка](../../../Winx/Tests/wxControlStateTests.cpp) охватывает masks, порядок callbacks, consuming completion и clone. Полная игровая сцена остаётся внешней зависимостью.
