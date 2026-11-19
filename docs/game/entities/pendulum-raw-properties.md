# Два DWORD-свойства Pendulum (PC)

`wxPendulum` предоставляет captured-value API для двух callback-пар собственного publisher: `Period (ms)` и `Damage`. Factory/RTTI receiver, текущие primary, RTTI, typed clone и регистрация связывают descriptors с классом. Короткие callbacks используются также другими классами; исключительное владение их кодом не предполагается.

| Descriptor | Смещение | Setter PC | Getter PC |
| --- | --- | --- | --- |
| Period (ms) | `+0x128` | `0x555060` | `0x555070` |
| Damage | `+0x84` | `0x57E390` | `0x580BE0` |

Setter переносит четыре байта аргумента из стека в поле. Getter возвращает четыре байта в EAX. Метки descriptors не устанавливают signedness, native C++ типы, единицы или применение значений в игре. Адреса setter и getter каждой пары проверены независимо.

Профиль `PcCapturedPrimaryRawDwords084128` принимает предоставленный префикс300байт с primary `0x7015BC`. Это минимальный префикс двух выбранных пар; историческая allocation352байта не назначает текущий sizeof или физическое наследование. `Max angle(rads)` по`+0x124` и `Initial offset(rads)` по`+0x150` с FLD getters исключены из RAW API; их отдельный scalar profile описан ниже. Байты всех невыбранных полей сохраняются.

`ReadRawPropertyForAnalysis` возвращает raw DWORD через выходной параметр. `WriteRawPropertyForAnalysis` сохраняет остальные байты и допускает alias входного/выходного префиксов. `ComposeRawPropertiesForAnalysis` выполняет GET/GET/SET/GET/GET/SET/GET/GET и выдаёт шесть raw значений плюс два состояния префикса. Возвращаемый `bool` — допуск собственного value API, а не native return signature. Неизвестные профиль, primary или selector отклоняются без изменения выхода.

Текущие constructor, factory и publisher этим API не выполняются. Полный layout, native signatures, lifetime, тайминг и движение маятника, применение урона, production dispatch, PS2, gameplay и целый класс остаются открытыми.

## Две F32 callback-пары

Отдельный scalar API использует `CapturedScalarPrefixForAnalysis` размером340байт (`0x154`) с primary `0x007015BC`. Тип и API прежнего RAW-префикса300байт (`0x12C`) сохранены. Историческая allocation352байта не является текущим `sizeof`.

| Метка publisher | Смещение | Setter PC | Getter PC |
| --- | --- | --- | --- |
| Max angle (rads) | `+0x124` | `0x00555040` | `0x00556E30` |
| Initial offset (rads) | `+0x150` | `0x00540CC0` | `0x00558A30` |

Scalar profile принимает finite normal binary32 и signed zero в обоих выбранных полях и аргументе setter. Setter копирует четыре raw байта аргумента; getter выполняет `FLD` binary32 и предоставляет десять байт little-endian extended payload с сохранением знака нуля. Для точного widening используется общий `FromFloat` из `spFloat80TowardZero.h`.

`ReadScalarForAnalysis` и `WriteScalarBitsForAnalysis` сохраняют выход при отказе. Scalar setter допускает alias входного и выходного340-байтных префиксов и сохраняет все байты кроме выбранного DWORD, включая оба независимых RAW-поля. Прежние RAW операции принимают arbitrary DWORD и непрозрачные прочие байты независимо от scalar domain.

Retained scalar composition задаётся GET, SET, GET, повторным SET того же аргумента, GET для каждой пары в порядке publisher. Она содержит шесть getter payload и четыре полных after-set префикса; store может не менять байты. Метки не задают диапазоны или механику маятника. Native C++ signatures, FPU environment, constructor, полный layout, lifetime, production caller, PS2, gameplay и целый класс требуют отдельных контрактов.
