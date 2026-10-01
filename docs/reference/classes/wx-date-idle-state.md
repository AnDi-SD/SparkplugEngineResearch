# `wxDateIdleState`

`wxDateIdleState` (Class ID `49D303E4`) — потомок [wxCharacterState](wx-character-state.md) с selector `34` (десятичный), исходным размером `3C` и без дополнительных полей. [Переносимая реализация](../../../Winx/Code/wxDateIdleState.cpp) не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `4043A0` | `3EFDA0` |
| Vtable | `6FA148` | `498FA0` |
| Вход, PC slot `1C` | `524090` | `310D10` |
| Выход, PC slot `20` | `524200` | `310B80` |
| Обновление, PC slot `30` | `5240C0` | `310BD0` |

Вход вызывает виртуальное обновление и возвращает `true`, без предварительного release. Обновление задаёт `(key & FF800059) | 59`, выполняет lookup. Только при различии handle освобождает старый, запускает новый с `mode=true`, `interrupt=true` и сохраняет после playback. Совпавший handle оставляет playback прежним. Control word не изменяется.

Выход сначала читает слово владельца `24`, затем отправляет сообщение `27ED` с параметром `1D`, source — само состояние. Два payload word содержат selector и прочитанное слово владельца. Оба передаются по значению на PC и PS2. После доставки вызывает общий выход базы, который читает актуальный pending и освобождает его. Callback сообщения может изменить pending до этого release.

Общий [hook выхода](../../../Winx/Code/wxDateStateOperationsForAnalysis.h) используется всеми тремя Date-состояниями. Доставку сообщения и доступ к слову владельца предоставляет обязательный [wxDateStateHost](../../../Winx/Analysis/Host/wxDateStateHost.h). Остальные hooks, Reset и пустой Copy унаследованы. Clone создаёт начальное состояние, деструктор не останавливает playback. [Проверка](../../../Winx/Tests/wxDateStateTests.cpp) включает callback с изменением pending; полный dispatcher остаётся внешней зависимостью.
