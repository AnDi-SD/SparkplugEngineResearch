# wxStickingLeftState и wxStickingRightState

Оба класса физически и регистрационно наследуются от `wxCharacterState`.
PC и PS2 выделяют по `0x3C` байт: дополнительных полей нет.

| Класс | Class ID | Selector `10` | Mode |
| --- | --- | --- | --- |
| wxStickingLeftState | `5A907EA0` | `12` | `30` |
| wxStickingRightState | `5C5147D6` | `13` | `40` |

Исходники: [Left.h](../../../Winx/Code/wxStickingLeftState.h),
[Left.cpp](../../../Winx/Code/wxStickingLeftState.cpp),
[Right.h](../../../Winx/Code/wxStickingRightState.h),
[Right.cpp](../../../Winx/Code/wxStickingRightState.cpp).
Общие операции сохранены в
[аналитическом helper](../../../Winx/Code/wxStickingStateOperationsForAnalysis.h),
который не является дополнительной физической базой игры.

Вход `1C` двухфазный. Первая фаза по once-флагу `1C` применяет
`(key & FFA7803F) | 00200030` для Left либо
`(key & FFA7804F) | 00200040` для Right. Читает packed word `owner13C`
на PC / `owner148` на PS2. При его младшем nibble `5` задаёт подполе variant
`1` в bits `23..27`, иначе `0`. Ищет handle и запускает mode=false,
interrupt=true **без освобождения прежнего pending**, сохраняет новый handle,
очищает `1C`, обнуляет control word `4` и возвращает false. Повторный вход
потребляет completion даже для null pending. При завершении вызывает базовый
вход с виртуальным update; иначе очищает control word и возвращает false.

Выход `20` по once-флагу `1E` сначала освобождает pending и читает packed word
`owner144` на PC / `owner150` на PS2. При младшем nibble `5` сразу вызывает
базовый выход, не меняя once-флаг и не подготавливая movement tracker.
Иначе вызывает общую подготовку движения с аргументом false и задаёт key:

| Класс | Первая маска выхода |
| --- | --- |
| Left | `(key & F05FFFBF) | 00400030` |
| Right | `(key & FFDFFFCF) | 00400040` |

Right **повторно** читает packed word выхода после подготовки и задаёт variant
`1`, если `(word & 00180000) == 00100000`; иначе variant `0`.
Left дополнительного чтения не делает. Оба ищут и запускают mode=false,
interrupt=true, сохраняют handle и очищают `1E`. Затем вызывают общую обработку
смещения с аргументом false, очищают control word и возвращают false.
Повторный выход потребляет completion. При завершении снова подготавливает
movement tracker и вызывает базовый выход; при ожидании обрабатывает смещение,
очищает control word и возвращает false.

Update `30` захватывает angle object и control object до сравнения.
PC использует `owner124->15C` и `owner12C->4`, PS2 — `owner130->168` и
`owner138->4`. PC читает сначала угол, затем motion; PS2 читает их в обратном
порядке. При motion `< 0.2` задаёт variant `0`.
Иначе Right при angle `<= 0.7853981852531433` задаёт consumer speed `1.3`
и variant `1`. Left делает то же при angle выше своего порога или равном ему.
Если условие угла не выполнено, прежний variant сохраняется.

Порог Left различается между платформами. PC умножает float32
`0.7853981852531433` на `3` без промежуточной записи float32;
PS2 использует отдельный float32 `4016CBE4` (`2.356194496154785`). Поэтому угол
`4016CBE4` включает boost на PS2 и не включает его на PC. Переносимая версия
сохраняет это различие численных профилей.

Затем update применяет `(key & FF87FFBF) | 30` для Left либо
`(key & FF87FFCF) | 40` для Right. При смене handle освобождает pending,
запускает mode=true, interrupt=true и сохраняет результат. Boost может
вызываться и при неизменном handle: setter расположен до lookup.

[Host](../../../Winx/Analysis/Host/wxStickingStateHost.h) отделяет внешние слова,
объекты, speed setter и node API. Movement использует поля общей базы
`28/2C/30..38`, описанные у [wxCharacterState](wx-character-state.md).
Copy пуст, clone создаёт новое состояние с полями конструктора. PS2 численные
проверки ограничены обычными конечными float32 и знаковыми нулями; полная
интеграция owner, consumer и EE accumulator для повёрнутого смещения открыта.

Проверка — `WinxStickingStateTests`: две фазы переходов, разные variant-ветви,
сохранение pending на первом входе, movement cache, speed setter и различие
порогов PC/PS2.
