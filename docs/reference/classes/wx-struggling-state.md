# `wxStrugglingState`

`wxStrugglingState` (Class ID `21B84A72`) — физический потомок [wxCharacterState](wx-character-state.md), selector `21` (33 в десятичной записи), native size `3C`; дополнительных полей нет. [Реализация](../../../Winx/Code/wxStrugglingState.cpp) сохраняет собственные entry/exit/update. Переносимый объект не задаёт native ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Entry, PC slot `1C` | `524090` | `2D42B0` |
| Exit, PC slot `20` | `51F8B0` | `2D4170` |
| Update, PC slot `30` | `51F8E0` | `2D4190` |

Entry вызывает виртуальный update и возвращает `true`, без предварительного release. Exit вызывает общий release с `forceStop=false` и возвращает `true`. Update не меняет caller-owned request: сбрасывает action control word, вызывает reset внешнего контроллера, делает lookup с постоянным key `20000`. Если handle изменился, выполняет release → mode-one queue с interrupt → store pending. При прежнем handle оба сброса всё равно выполняются, а playback остаётся прежним.

PC reset вызывается по адресу `4D96A0` для borrowed `owner+124 -> entity+12C`. Это защищённая внешняя служба; её полное PC-тело не объявляется восстановленным. PS2 использует `owner+130 -> entity+138` и обнуляет пятнадцать слов inline: `1C8/1CC/1D0`, `1BC/1C0/1C4`, `1D4/1D8/1DC`, `1A0/1A4/1A8`, `194/198/19C`. Исходные имена контроллера и групп остаются неизвестными.

Обязательный [wxStrugglingStateHost](../../../Winx/Analysis/Host/wxStrugglingStateHost.h) передаёт внешнюю службу reset и остальные borrowed операции. Constructor/RTTI/factory, default Clone и пустой Copy используют общий протокол. Reset базового состояния не вызывает stop/fade; прочие hooks унаследованы.

[Проверка](../../../Winx/Tests/wxStrugglingStateTests.cpp) охватывает entry → повторный update с прежним handle → exit, порядок reset/lookup/release, постоянный key, RTTI/Clone/Copy/Reset. Полный игровой runtime и защищённое PC-тело reset остаются открытыми.
