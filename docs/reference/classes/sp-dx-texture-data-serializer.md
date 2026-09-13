# spDXTextureDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXTextureDataSerializer](../../../Sparkplug/Code/Sparkplug/spDXTextureDataSerializer.h).

PC сохраняет точный путь translation unit:

`Z:\Sparkplug\Code\Sparkplug\spDXTextureDataSerializer.cpp`

PS2 сохраняет `spDXTextureDataSerializer.cpp` и имя класса. Это показывает, что DX-ветка
входила и в общий PS2 source set, хотя её runtime target является платформенным типом DX.

## Методы

PS2 secondary thunks `0x00176310/0x00176300/0x001762F0` корректируют `this-0x10`
для write/index/read. Index pass всегда успешен: нативный блок содержит собственные
байты, а не resource relationships.

## Общий wrapper и platform type

Сначала вызывается source writer базового `spTextureDataSerializer`. Embedded stream
завершает запись полем 3, referenced stream — полем 4. Для локального source поле 2
открывает тело в порядке:

1. поле 6 `PlatformType`;
2. поле 0 `CrossPlatform`, только для native mode `0` или `2`;
3. поле 1 `PlatformSpecific`, всегда.

Значение platform type равно `7` для mode `0/2` и `6` для остальных наблюдаемых
режимов. Reader dispatch-ит поля `0`, `1`, `6`, неизвестные поля пропускает. При
выключенном native-флаге он читает cross-platform и пропускает native payload, при
включённом делает обратное. Маски не объединены: PC использует `0x02`, PS2 — `0x08`.

## payload поля 1

Внутри platform-specific поля расположен вложенный datablock с двумя field ID:

- `0` (`esfDXTextureData`) — первый mip и общий префикс;
- `1` (`esfDXTextureMipmap`) — каждый следующий mip.

Поле 0 пишет byte-флаг platform-specific data, затем `width`, `height`, `pixelFormat`,
byte-флаг pixel data, после чего первый mip. Каждый mip имеет wire-порядок:

1. `uint32 width`;
2. `uint32 rowStride`;
3. `uint32 height`;
4. `rowStride * height` raw bytes.

Количество mip-уровней задаётся числом полей, отдельного count в потоке нет. Writer
требует хотя бы один mip; пустой контейнер заканчивается диагностикой ошибки. В памяти
одна запись занимает `0x10`: width, height, rowStride и pointer. PC хранит vector-подобный
begin/end/capacity у `spTextureData + 0x6C`; PS2 использует иной контейнер. Эти layouts
не смешаны в portable-классе.

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямой PC `sizeof` и распаковка protected factory entry.
3. Original enum/name native mode, reader flags и pixel formats.
4. Семантика первого byte-флага и допустимые комбинации pixel-data flag/mip records.
5. Полный Direct3D format/FVF mapping, texture creation, pitch conversion и ownership.
6. Alignment, status enum и rollback при частично прочитанной mip-chain.
7. Виртуальный identifier0B1C67BB и отсутствие отдельного RTTI-класса: не
   смешивать wire78EA082B, temporaryTextureData и runtimeDXTexture3F3651B6.
