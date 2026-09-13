# spPCFontManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spFontManager](../../../Sparkplug/Code/Sparkplug/spFontManager.h), [spPCFontManager](../../../Sparkplug/Code/SparkplugPC/spPCFontManager.h).

Exact original translation unit:
`Z:\Sparkplug\Code\SparkplugPC\spPCFontManager.cpp`.

The PC registration at `0x007647D0` identifies class `0x7B467097`, direct base
`spFontManager / 0x1640375E`, initializer `0x006D5790`, and factory
`0x004C35A0`. The factory allocates exactly `0x3C`, calls the common
constructor entry and installs primary/support vtables
`0x006F25C4/0x006F25C0`. The leaf adds no storage.

Vtable anchors are registration getter `0x004C3590`, destructor
`0x004C3620` and clone `0x004C3650`. Clone constructs a blank manager and uses
the inherited empty base-copy path, so fonts and backend state are not copied.
