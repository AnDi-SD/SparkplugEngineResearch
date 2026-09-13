# spPS2ErrorManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2ErrorManager](../../../Sparkplug/Code/SparkplugPS2/spPS2ErrorManager.h).

Статус: identity/base, exact size, обе vtable, factory/direct-create/clone,
handler provider и фактически пустой callback подтверждены по retail ELF.
Original source path и header не найдены; путь `Code/SparkplugPS2` inferred.

## Identity и адреса

| Поле | Значение |
| --- | ---: |
| Class ID | `0x226A416D` |
| Base | `spErrorManager / 0x660E40D8` |
| Provider / callback | `0x0020DA30` / `0x0020DA50` |
| Format adapter | `0x0020DA40` |

Оба create paths выделяют `0x424`, вызывают common constructor `0x00107CB0` и
заменяют два vptr. Derived storage отсутствует. Clone создаёт такой же объект,
использует общую clone map и inherited empty copy.

Provider `0x0020DA30` буквально возвращает `0x0020DA50`; callback состоит из
`jr ra; nop` и ничего не выводит. Дополнительный support-table adapter
`0x0020DA40` переставляет register arguments и переходит в common formatter
`0x00107950`; его original signature пока не известна.

## Границы описания

Открыты original TU/header, имя дополнительного adapter slot, причина пустого
retail handler (возможен platform debug build с иной реализацией) и связь
fatal common target `0x00403A88` с системным shutdown/assert path.
