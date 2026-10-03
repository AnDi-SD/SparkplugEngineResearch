# wxKnutAttackState

Состояние с class ID `2FDC3901`, физической и регистрационной базой
`wxCharacterState`. PC и PS2 выделяют `0x3C` байт; селектор равен `3`.
Собственных полей нет. Реализация: [заголовок](../../../Winx/Code/wxKnutAttackState.h)
и [методы](../../../Winx/Code/wxKnutAttackState.cpp).

Update `30` работает однократно до Reset, по флагу `1D` базы. Сначала применяет
`key &= FF987F8F`, затем выбирает вариант по внешнему байту `1F`:
при ненулевом байте `(key & F0FFFFFF) | 00800000`, иначе `key & F07FFFFF`.
PC читает байт через `owner124->130`, PS2 — через `owner130->13C`.
При смене handle освобождает старый pending, запускает новый с mode=false,
interrupt=true и сохраняет handle после playback. Флаг `1D` очищается после
этих операций и при совпадающем handle. Повторный update не меняет ключ.

Permission `34` сразу разрешает code `A` и null pending. Иначе потребляет
completion-записи общей службы. Вход, выход, Reset и Copy унаследованы;
clone получает свежие поля базы и selector конструктора.

События обрабатываются с учётом регистра. Первые одиннадцать совпадений точные;
после них проверяются подстроки `begin`, `scepter_start`, `scepter_end` в этом
порядке. Поэтому, например, `event_hurt_begin_suffix` попадает в общий `begin`.

| Событие | Код / действие | Известный payload0 | Известный payload1 |
| --- | --- | --- | --- |
| `event_turnleft_begin` | `273C` | byte `1` | byte `0` |
| `event_turnleft_end` | `273C` | byte `0` | byte `0` |
| `event_turnright_begin` | `273C` | byte `1` | byte `1` |
| `event_turnright_end` | `273C` | byte `0` | byte `1` |
| `event_turn_begin` | `273C` | byte `1` | byte `0` |
| `event_turn_end` | `273C` | byte `0` | byte `0` |
| `event_defense_begin/end` | `2744` | byte `1/0` | word `0` |
| `event_charge_begin` | `2745` | byte `1` | word `0` |
| `event_hurt_begin/end` | `2746`, затем `274C` | byte `1/0` | word `0`, затем `3/0` |
| Подстрока `begin` | controller slot `38(1)`, затем `27D1` | word `12` | word `0` |
| Подстрока `scepter_start/end` | controller slot `3C(1/0, 0)` | — | — |

Все коды и slots шестнадцатеричные, значения payload десятичные.
Уведомления получает `owner24`; второе уведомление hurt получает
`wxGameCore` field `2B4`, независимо от наличия первого получателя.
Null-получатель пропускает dispatch. Controller-ветви собственного null-guard
не имеют. PS2 controller slots соответствуют `40/44`.

[wxKnutAttackStateHost](../../../Winx/Analysis/Host/wxKnutAttackStateHost.h) — наш
обязательный адаптер заимствованных объектов. Payload, записанный как byte,
имеет неизвестные верхние 24 бита; адаптер передаёт маску известных битов и
не объявляет padding нулевым. Проверка — `WinxKnutAttackStateTests`, включая
порядок callback, однократность update, Reset, completion и clone. Полная
интеграция контроллеров и игровых получателей пока не восстановлена.
