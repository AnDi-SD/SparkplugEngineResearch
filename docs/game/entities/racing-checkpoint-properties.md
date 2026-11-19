# Два DWORD-свойства RacingCheckpoint (PC)

`wxRacingCheckpoint` предоставляет captured-value API для двух callback-пар, связанных его собственным publisher: `Bonus Time (ms)` и `Checkpoint ID`. Сохранённый factory/RTTI receiver и текущие primary, RTTI, typed clone и регистрация связывают эти descriptors с данным классом. Сами короткие callback bodies используются также другими классами; они не считаются исключительными методами RacingCheckpoint.

| Descriptor | Смещение | Setter PC | Getter PC |
| --- | --- | --- | --- |
| Bonus Time (ms) | `+0x138` | `0x5100F0` | `0x57FC10` |
| Checkpoint ID | `+0x128` | `0x555060` | `0x555070` |

Setter читает четыре байта аргумента из стека и записывает DWORD без преобразования. Getter возвращает четыре прочитанных байта в EAX. Имена descriptors не устанавливают signedness, единицы или применение значения в игре. Setter и getter одной пары могут находиться далеко друг от друга.

Профиль `PcCapturedPrimaryRawDwords128138` принимает предоставленный префикс 316 байт с primary `0x701188`. Это минимальный префикс выбранных двух callback-пар. Историческая allocation была 356 байт; текущий native sizeof и физическое наследование не установлены. `Is Finish Line?` по `+0x140` находится вне 316-байтного префикса и не входит в этот RAW DWORD API.

`ReadRawPropertyForAnalysis` возвращает raw DWORD через выходной параметр. `WriteRawPropertyForAnalysis` сохраняет остальные байты и допускает alias входного и выходного префиксов. `ComposeRawPropertiesForAnalysis` воспроизводит порядок GET/GET/SET/GET/GET/SET/GET/GET с шестью значениями и двумя состояниями префикса. Возвращаемый `bool` обозначает допуск собственного value API и не назначает native C++ return signature. Неизвестный профиль, неверный primary или неизвестный selector отклоняются без изменения выхода.

Конструктор, factory, publisher и остальные методы не вызываются этим API. Полный layout, native signatures, lifetime, production dispatch, обработка финиша и racing progression, PS2 и gameplay остаются открытыми.

## Отдельное байтовое свойство Finish (PC)

Собственный publisher `wxRacingCheckpoint` связывает descriptor `Is Finish Line?` с setter `0x57EE80` и getter `0x57EE90` по смещению `+0x140`. Setter читает один байт из аргумента по `[ESP+4]` и записывает его в receiver. Getter загружает этот байт в AL. Оба тела сохраняют верхние 24 бита EAX; значения `0x02`, `0x80` и `0xFF` не нормализуются к Boolean. Это описание инструкций не устанавливает native C++ return signature или семантику прохождения финиша.

`CapturedFinishPrefixForAnalysis` содержит отдельный предоставленный префикс 321 байт. Профиль `PcCapturedPrimaryFinishLowByte140Prefix141` проверяет primary `0x701188` и допускает `ReadFinishByteForAnalysis`, `WriteFinishLowByteForAnalysis` и `ComposeFinishLowByteForAnalysis`. Последовательность GET/SET/GET возвращает два raw byte значения и полный after-prefix; setter использует только low8 переданного `uint32` слова и сохраняет остальные байты. Он допускает один массив как вход и выход. Неверный primary, неизвестный или неподходящий профиль оставляют выход без изменения. Возвращаемый `bool` сообщает допуск собственного value API.

Прежние `CapturedPrefixForAnalysis` размером 316 байт, RAW DWORD профиль и методы сохраняют свой самостоятельный контракт. Префикс Finish не расширяет этот тип. Размер 321 задаёт только сохранённый extent выбранной байтовой пары; историческая allocation 356 байт отдельно известна из factory/RTTI наблюдения и не является доказательством текущего native sizeof, конструктора или полного layout. Конструктор, factory, publisher и полный clone новым фасадом не исполняются. Native upper-EAX остаётся диагностикой оригинальных вызовов и не возвращается value API. Production callers, обработка финиша, lifetime, физическое наследование, PS2 и gameplay требуют отдельных проверок.
