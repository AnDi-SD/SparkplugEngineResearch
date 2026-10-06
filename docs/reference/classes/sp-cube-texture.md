# spCubeTexture

`spCubeTexture` (`0x65557907`) — общая абстрактная база кубической текстуры. Регистрационная и физическая база — [`spTexture`](sp-texture.md). [Исходники](../../../Sparkplug/Code/Sparkplug/spCubeTexture.h) сохраняют собственный RTTI, отсутствие фабрики и null Clone; аппаратные методы остаются границей backend.

Объект PC и PS2 занимает `0x38` байт и не добавляет полей к `spTexture`. PC primary table `0x006F1F50` содержит null Clone `0x004A1BF0` и наследованное копирование `spNamedObject` `0x00413120`. PS2 primary table `0x0048EAD0` аналогично содержит null Clone `0x00173920` и Named copy `0x00105DC0`. Конструктор PS2 `0x001738E0` вызывает базовый конструктор текстуры и заменяет обе таблицы.

База самостоятельно не создаёт изображения и не загружает GPU storage. PC реализация находится в [`spDXCubeTexture`](sp-dx-cube-texture.md); PS2 оболочка с оригинальными аппаратными stubs — в [`spPS2CubeTexture`](sp-ps2-cube-texture.md).
