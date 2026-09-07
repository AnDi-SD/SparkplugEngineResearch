# `spTextureDataSerializer`: источники и cross-platform текстура

Статус: точный source identity, RTTI/lifetime, storage-free ABI, target,
source-wrapper, base write/read dispatch и арифметика cross-platform payload
подтверждены независимо на PC и PS2. Загрузка внешнего ресурса и настоящий
stream codec первоначально были evidence-only. PC checkpoint14 добавил
исполненный CPU source/local/raw codec и byte-exact comparison:
[карточка](native-pc-texture-codec-boundaries.md). External source ещё открыт.
PC checkpoint 16 перенёс общий recursive source wrapper и ограниченный DX
native-data reader: [карточка](native-pc-texture-native-source.md).

PC сохраняет точный путь translation unit:

`Z:\Sparkplug\Code\Sparkplug\spTextureDataSerializer.cpp`

PS2 независимо содержит имя `spTextureDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x1C4C75BA`, C++ и registered base — direct `spSerializer`
(`0x42429877`), target — `spTextureData::ClassID` (`0x78EA082B`).

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x0075E528` | `0x004A9BB0` |
| Initializer | `0x006D2C60` | `0x00482ED0` |
| Factory | `0x0042DC30` (protected entry) | `0x00178BA0` |
| Allocation | observed `0x14` | exact `0x14` |
| Primary vtable | `0x006DDD90` | header `0x0048ECF0` |
| Secondary vtable | `0x006DDD84` | header `0x0048ED14` |

PS2 factory выделяет ровно `0x14` байт, вызывает общий constructor
`spSerializer` и меняет только два vptr. PC destructor аналогично возвращает
secondary vptr к таблице этого класса и переходит в base destructor; новых
полей состояния не наблюдается.

## Методы

| Роль | PC | PS2 |
|---|---:|---:|
| registration getter | `0x0042DBF0` | `0x00177830` |
| read source wrapper | `0x0042EA50` | `0x00177840` |
| write source wrapper | `0x0042E5F0` | `0x00177B30` |
| read cross-platform texture | `0x0042E100` | `0x00177F00` |
| write cross-platform texture | `0x0042DD70` | `0x00178200` |
| header → runtime factory (PC: DXTexture) | `0x0042DD10` | `0x00178980` |
| write | `0x0042EE10` | `0x00178640` |
| index resource graph | `0x005A7DB0` | `0x00178460` |
| read | `0x0042F180` | `0x00178470` |
| target class ID | `0x0042DC20` | `0x00178A00` |
| deleting destructor | `0x0042DCF0` | `0x00178A10` |
| blank clone | `0x0042DCA0` | `0x00178AC0` |

PS2 secondary thunks `0x00178CC0/0x00178CB0/0x00178CA0` адаптируют write,
indexing и read. Index pass возвращает success без добавления relationships.

## Source-wrapper

Диагностические имена и оба call graph подтверждают поля:

| ID | Имя | Значение |
|---:|---|---|
| 2 | `esfTextureDataSourceNone` | внешний source stream отсутствует |
| 3 | `esfTextureDataSourceEmbeded` | source — вложенный `spMemoryStream` |
| 4 | `esfTextureDataSourceReference` | source — ссылка на обычный `spStream` |

Оригинал действительно пишет `Embeded` с одной буквой `d`. Writer проверяет
attached source через RTTI: сначала `spStream` (`0x6CC80D8A`), затем
`spMemoryStream` (`0x57177DB5`). Для memory stream он временно сохраняет
позицию, копирует вложенный поток в field 3 и восстанавливает позицию. Для
прочего stream он формирует reference path и пишет field 4. Обе ветви
полностью завершают payload текущего объекта.

Если source отсутствует, writer пишет field 2 со значением `false`, но это не
означает пустую текстуру: вызывающий код продолжает записывать локальное
представление объекта. Reader зеркально различает `2/3/4`; reference-ветка
создаёт и открывает file stream относительно текущего ресурса, embedded-ветка
повторно вызывает serializer на текущем потоке, а None возвращает управление
локальному reader. Неизвестные поля безопасно пропускаются.

## Базовое локальное представление

После `SourceNone` базовый writer проверяет пока не названный native mode.
Только режимы `0` и `2` пишут:

1. field 6 `esfTextureDataPlatformType`, значение `1`;
2. field 0 `esfTextureDataCrossPlatform`;
3. внутри него raw texture field 5.

Source wrapper имеет собственный terminator после field2, локальная секция —
отдельный. В других режимах базовый writer пишет пустую локальную секцию.
Значение platform type `1` поэтому сохранено как числовой подтверждённый
контракт, а не как придуманный enum. Base reader после source-wrapper знает
только field 0; platform-specific field 1 относится к производным
`spDXTextureDataSerializer`/`spPS2TextureDataSerializer`.

Raw field 5 последовательно содержит четыре `UInt32` и байты:

```text
width
height
pixelFormat
pixelSize
byte pixels[width * height * pixelSize]
```

Код обеих платформ намеренно не включает `spTextureBuffer::depth` в этот
размер. Portable `CrossPlatformPayloadHeader` повторяет именно эту арифметику,
проверяет 32-bit overflow и наличие требуемого числа байт. PC checkpoint14
восстановил именно CPU stream writer; это не полный DX/external-source writer.

## Portable-срез и проверка

`Code/Sparkplug/spTextureDataSerializer.*` восстанавливает RTTI/factory,
blank clone, target ID, точные field IDs, source/mode write plan и безопасный
header planner. Историческая проверка: последовательная `ninja -j1` сборка и
два тогдашних CTest-набора проходили.
Тесты закрепляют direct base, exact PS2/observed PC `0x14`, три source-ветки,
режимы `0/2`, raw header `3×5×4 = 60` и отказ для неинициализированного буфера.

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. PC factory42DC30 теперь исполнен, exact allocation14; полный rollback/failure ещё открыт.
3. Имена native mode и platform type enum.
4. Attached-source member/API, правила canonical reference path и resolver.
5. Точный status/rollback при ошибке вложенного либо внешнего потока.
6. Platform container mapping и производные DX/PS2 texture serializers.

Checkpoint14:42DD10 потребляет8 arbitrary bytes и создаёт actual DXTexture4AB520,
игнорируя оба слова; это не generic467550. На checkpoint14 source был
явно unavailable; checkpoint15 теперь создаёт правильный DXTexture CPU shadow,
без заявления о live COM.3 tiny CPU read/write
rows exact, texture source113 checks, CTest32/32; platform header/shared
source/lifetime границы описаны в новой карточке. Старые PS2 выводы не повышались.

Actual PC startup использует wire78EA082B для Data/DXData/PS2Data serializers
с разными platform masks; virtual target identifier нельзя использовать
как единственный registry key. Field3 вызывает **тот же selected serializer
read рекурсивно**, без нового8-byte object header:
[runtime/mips/registry](native-pc-texture-runtime-mips.md).
