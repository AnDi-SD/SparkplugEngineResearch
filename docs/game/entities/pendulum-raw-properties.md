# Два DWORD-свойства Pendulum (PC)

`wxPendulum` предоставляет captured-value API для двух callback-пар собственного publisher: `Period (ms)` и `Damage`. Factory/RTTI receiver, текущие primary, RTTI, typed clone и регистрация связывают descriptors с классом. Короткие callbacks используются также другими классами; исключительное владение их кодом не предполагается.

| Descriptor | Смещение | Setter PC | Getter PC |
| --- | --- | --- | --- |
| Period (ms) | `+0x128` | `0x555060` | `0x555070` |
| Damage | `+0x84` | `0x57E390` | `0x580BE0` |

Setter переносит четыре байта аргумента из стека в поле. Getter возвращает четыре байта в EAX. Метки descriptors не устанавливают signedness, native C++ типы, единицы или применение значений в игре. Адреса setter и getter каждой пары проверены независимо.

Профиль `PcCapturedPrimaryRawDwords084128` принимает предоставленный префикс300байт с primary `0x7015BC`. Это минимальный префикс двух выбранных пар; историческая allocation352байта не назначает текущий sizeof или физическое наследование. `Max angle(rads)` по`+0x124` и `Initial offset(rads)` по`+0x150` с FLD getters исключены. Байты всех невыбранных полей сохраняются.

`ReadRawPropertyForAnalysis` возвращает raw DWORD через выходной параметр. `WriteRawPropertyForAnalysis` сохраняет остальные байты и допускает alias входного/выходного префиксов. `ComposeRawPropertiesForAnalysis` выполняет GET/GET/SET/GET/GET/SET/GET/GET и выдаёт шесть raw значений плюс два состояния префикса. Возвращаемый `bool` — допуск собственного value API, а не native return signature. Неизвестные профиль, primary или selector отклоняются без изменения выхода.

Текущие constructor, factory и publisher этим API не выполняются. Полный layout, native signatures, lifetime, тайминг и движение маятника, применение урона, production dispatch, PS2, gameplay и целый класс остаются открытыми.
