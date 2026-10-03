# wxBloomDyingState

`wxBloomDyingState` — отдельный leaf `wxCharacterState`, selector `11`.
Регистрационное имя не означает физического наследования от
[wxDyingState](wx-dying-state.md): оба класса непосредственно используют
[wxCharacterState](wx-character-state.md).

| Свойство | PC | PS2 |
| --- | --- | --- |
| Class ID | `0x15526B2C` | `0x15526B2C` |
| Native размер | `0x40` | `0x40` |
| Собственное поле | byte `+0x3C`, default `0` | то же |
| Entry, slot `+0x1C/+0x24` | `0x005134B0` | `0x002C8370` |
| Update, slot `+0x30/+0x38` | `0x00513590` | `0x002C8210` |
| Permission `+0x34/+0x3C` | собственный byte | собственный byte |
| Permission `+0x38/+0x40` | всегда true | всегда true |

Clone оставляет собственный byte в default `0`; inherited copy и reset
сохраняют уже имеющийся собственный byte назначения.

Entry вызывает внешний manager method PC `0x00593B00` / PS2 `0x00365450`
с аргументами `{7000,0}`, затем устанавливает consumer speed `1`, после чего
обнуляет собственный byte. Packed key сохраняет low nibble только при
значениях `1` или `3`; для прочих значений он становится нулём. Следующий
этап: `(key & 0xFFF9800F) | 0x18000`.

При ненулевом owner byte PC `+0x284` / PS2 `+0x2A0` применяется
`(key & 0xFF97FFF0) | 0x100000`; иначе `key &= 0xFF87FFFF`.
Затем во всех случаях `key &= 0xF07FFFFF`. Entry разрешает animation handle,
queue использует `mode=false, interrupt=true`, новый handle записывается в
pending перед virtual update. Старый handle предварительно не освобождается.

Update сначала читает gate PC `owner+0x124->0x148` / PS2
`owner+0x130->0x154`. Ненулевое значение немедленно прекращает операцию.
При нулевом gate вызывается virtual reset внешнего control, затем completion
query с consumption `true`, затем проверяется собственный byte.
Даже при уже отправленном сообщении completion records могут быть consumed.

При complete и нулевом byte последовательно выполняются внешние manager calls:

| PC method | PS2 method | Аргументы |
| --- | --- | --- |
| `0x00593A60` | `0x00365570` | `{1000,0}` |
| `0x00595450` | `0x0033A7A0` | `{0x35,1}` |
| `0x005953E0` | `0x0033A890` | `{0x40,0}` |

После этих вызовов заново читается `owner+0x24`. Ненулевой target получает
packet `{0x27E9,0,0,0,state,0,0,0}`; null пропускает доставку. Последним шагом
собственный byte становится `1`. Исходные имена manager methods и их полный
протокол остаются неизвестными; адресные обозначения не заменяются догадками.

Восстановлены собственные ветви класса, с отдельными foreign services в
`Winx/Analysis/Host/wxDyingStateHost.h`; реализация —
`Winx/Code/wxBloomDyingState.*`. Отсутствующий host явно отклоняется.
Полный менеджерный и scene/runtime путь этих borrowed объектов остаётся
за границами компонента.
