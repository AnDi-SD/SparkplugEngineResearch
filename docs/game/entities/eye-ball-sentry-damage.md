# Raw Damage у EyeBallSentry (PC)

RAW API `wxEyeBallSentry` моделирует callback-пару descriptor `Damage` собственного publisher. Setter `0x56B080` загружает четыре байта аргумента из стека и записывает их в `+0x144`; getter `0x57FC30` возвращает эти четыре байта в EAX. Адреса setter и getter установлены отдельно по descriptor. Название не задаёт native C++ тип, signedness, единицы или применение урона.

Профиль `PcCapturedPrimaryRawDword144` допускает предоставленный328-байтный префикс с primary `0x700DE8`. `ReadDamageRawForAnalysis`, `WriteDamageRawForAnalysis` и `ComposeDamageRawForAnalysis` сохраняют все32 бита. Compose выполняет GET/SET/GET и выдаёт прежнее значение, полный after-prefix и новое значение. Остальные байты сохраняются, input/output prefix могут совпадать. Неизвестный profile или primary отклоняется без изменения выхода. Возвращаемый `bool` описывает допуск нашего анализа и не устанавливает native signature.

Историческая allocation416байт и минимальный выбранный префикс328байт — разные границы. Factory/RTTI, primary, typed clone, регистрация и descriptor собственного publisher атрибутируют выбранную пару. Текущий FF25 constructor не выполняется и его тело неизвестно. RAW profile описывает выбранную Damage-пару. Byte/прочие формы descriptors требуют отдельных контрактов. Native signatures, sizeof/layout, физическое наследование, constructor/lifecycle, поведение sentry и применение урона, production callers, PS2, gameplay и целый класс остаются открытыми.

## Scalar angle and period callbacks

The separate scalar profile admits the supplied 328-byte prefix with primary `0x00700DE8` and finite binary32 normal values or signed zero in three selected fields and setter arguments. Each setter copies four raw argument bytes. Each getter loads binary32 with `FLD` and supplies the physical ten-byte little-endian extended payload, preserving signed zero.

| Publisher label | Captured offset | Setter | Getter |
| --- | --- | --- | --- |
| Angle Max rads ( += 0) | `+0x130` | `0x00555080` | `0x00555090` |
| Angle Min rads ( -= 0) | `+0x12C` | `0x005550A0` | `0x0057B8B0` |
| Oscillation period (s) | `+0x128` | `0x00555060` | `0x00580BD0` |

The API stages results, preserves output on refusal and permits prefix input/output aliasing for setters. Every other byte, including the independently admitted RAW Damage field, is preserved. The shared `FromFloat` provider in `spFloat80TowardZero.h` performs exact finite binary32 widening. The RAW API retains its arbitrary-bit Damage and opaque-prefix contract.

The retained scalar composition performs GET, SET, GET, SET of the same argument, GET for each property in publisher order. Its values contain nine getter payloads and six complete after-set prefixes per supplied state. Publisher spelling, angle labels and time labels do not impose ranges or additional arithmetic. Historical allocation416 and the selected328-byte prefix retain their separate meanings. Native C++ signatures, FPU environment and stack behavior, construction, lifetime, sentry mechanics and production callers require their own qualification.
