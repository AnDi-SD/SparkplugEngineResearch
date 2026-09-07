# `spDXTextureDataSerializer`: DX-текстура и mip-chain

Статус: точный source identity, RTTI/lifetime, storage-free ABI, target, source-wrapper,
writer/reader dispatch и каркас нативного payload подтверждены независимо в PC и PS2.
Полный native stream codec и Direct3D resource creation пока не перенесены.
PC checkpoint 16 восстановил native-data reader и общий source wrapper для
полных поддержанных mip chains, с 11 exact comparisons оригинальных пикселей
и состояния: [карточка](native-pc-texture-native-source.md). Writer, conversion,
missing mips, external source и live Direct3D по-прежнему открыты.
PC checkpoint18 добавляет [native-data writer](native-pc-texture-native-writer.md)
на CPU TextureData, policies0/1/2;6 byte-exact cases. Это не writer runtimeDXTexture.
PC checkpoint14 подтвердил actual factory14, header42DD10→DXTexture4AB520
и отдельные COM boundaries, без GPU:
[карточка](native-pc-texture-codec-boundaries.md).

PC сохраняет точный путь translation unit:

`Z:\Sparkplug\Code\Sparkplug\spDXTextureDataSerializer.cpp`

PS2 сохраняет `spDXTextureDataSerializer.cpp` и имя класса. Это показывает, что DX-ветка
входила и в общий PS2 source set, хотя её runtime target является платформенным типом DX.

## Идентичность и ABI

Class ID `0x1C6D480F`, direct C++/registered base — `spTextureDataSerializer`
(`0x1C4C75BA`). Virtual identifier — `0x0B1C67BB`, исторически подписанный
`spDXTextureData`. **Actual PC startup регистрирует этот serializer под wire
`spTextureData78EA082B`, masks6/op1 или2**, а не под0B1C67BB. Отдельный runtime
класс с таким ID этим getter не доказан. Исполненные registrations и реальные
runtime/native mip codecs: [checkpoint15](native-pc-texture-runtime-mips.md).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E460` | `0x004A9AF0` |
| Initializer | `0x006D2C00` | `0x00482E50` |
| Factory | `0x0042B660` (protected entry) | `0x00175D50` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006DD648` | header `0x0048EC30` |
| Secondary vtable | `0x006DD63C` | header `0x0048EC54` |

PS2 factory выделяет `0x14` байт, вызывает constructor базового serializer и заменяет
только два vptr. Derived-состояния нет. PC размер остаётся наблюдаемым из factory path,
поскольку прямой allocator был скрыт защитной секцией. PC checkpoint14
исполняет этот original factory и фиксирует actual allocation14.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0042B560` | `0x00174D60` |
| serialize native payload | `0x0042B9E0` | `0x00175060` |
| write | `0x0042BF40` | `0x001757B0` |
| index resource graph | `0x005A7DB0` | `0x00174D70` |
| load native payload | `0x0042C3B0` | `0x00174D80` |
| read | `0x0042C640` | `0x00175420` |
| target class ID | `0x0042B590` | `0x00175BF0` |
| destructor | `0x0042B570` | `0x00175C00` |
| deleting destructor / blank clone | `0x0042B720` / `0x0042B6D0` | ABI-combined / `0x00175C70` |

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

## Нативный payload поля 1

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

Восстановленный planner моделирует только доказанные поля и безопасно считает
`rowStride * height`, отвергая переполнение `uint32`. Он не выдаётся за загрузчик D3D
texture и не создаёт отсутствующие native mip records в переносимом `spTextureData`.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты фиксируют direct
base/target, terminal source cases, порядок `2 -> 6 -> 0? -> 1`, type `6/7`, отдельные
PC/PS2 reader masks, zero-mip failure, mip wire sizing, overflow guard, ABI и clone.

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. Прямой PC `sizeof` и распаковка protected factory entry.
3. Original enum/name native mode, reader flags и pixel formats.
4. Семантика первого byte-флага и допустимые комбинации pixel-data flag/mip records.
5. Полный Direct3D format/FVF mapping, texture creation, pitch conversion и ownership.
6. Alignment, status enum и rollback при частично прочитанной mip-chain.
7. Виртуальный identifier0B1C67BB и отсутствие отдельного RTTI-класса: не
   смешивать wire78EA082B, temporaryTextureData и runtimeDXTexture3F3651B6.

Checkpoint15 исполнил9 native mip readers с actual temporaryTextureData,
vector16 records и target4ABBA0→explicit COM row copy. Runtime44/size48
при этом **не заполняются**, хотя обычный runtime serializer4B2950 их
устанавливает. Полный источник, missing-mip generation и errors ещё открыты;
source adapter пока не заменён неподтверждённой конверсией.
