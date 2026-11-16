# Два DWORD-свойства RacingCheckpoint (PC)

`wxRacingCheckpoint` предоставляет captured-value API для двух callback-пар, связанных его собственным publisher: `Bonus Time (ms)` и `Checkpoint ID`. Сохранённый factory/RTTI receiver и текущие primary, RTTI, typed clone и регистрация связывают эти descriptors с данным классом. Сами короткие callback bodies используются также другими классами; они не считаются исключительными методами RacingCheckpoint.

| Descriptor | Смещение | Setter PC | Getter PC |
| --- | --- | --- | --- |
| Bonus Time (ms) | `+0x138` | `0x5100F0` | `0x57FC10` |
| Checkpoint ID | `+0x128` | `0x555060` | `0x555070` |

Setter читает четыре байта аргумента из стека и записывает DWORD без преобразования. Getter возвращает четыре прочитанных байта в EAX. Имена descriptors не устанавливают signedness, единицы или применение значения в игре. Setter и getter одной пары могут находиться далеко друг от друга.

Профиль `PcCapturedPrimaryRawDwords128138` принимает предоставленный префикс 316 байт с primary `0x701188`. Это минимальный префикс выбранных двух callback-пар. Историческая allocation была 356 байт; текущий native sizeof и физическое наследование не установлены. `Is Finish Line?` по `+0x140` находится вне выбранного префикса и не входит в этот API.

`ReadRawPropertyForAnalysis` возвращает raw DWORD через выходной параметр. `WriteRawPropertyForAnalysis` сохраняет остальные байты и допускает alias входного и выходного префиксов. `ComposeRawPropertiesForAnalysis` воспроизводит порядок GET/GET/SET/GET/GET/SET/GET/GET с шестью значениями и двумя состояниями префикса. Возвращаемый `bool` обозначает допуск собственного value API и не назначает native C++ return signature. Неизвестный профиль, неверный primary или неизвестный selector отклоняются без изменения выхода.

Конструктор, factory, publisher и остальные методы не вызываются этим API. Полный layout, native signatures, lifetime, production dispatch, FinishLine, racing progression, PS2 и gameplay остаются открытыми.
