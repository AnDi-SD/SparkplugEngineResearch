# spTextureDataSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTextureDataSerializer](../../../Sparkplug/Code/Sparkplug/spTextureDataSerializer.h).

PC сохраняет точный путь translation unit:

`Z:\Sparkplug\Code\Sparkplug\spTextureDataSerializer.cpp`

PS2 независимо содержит имя `spTextureDataSerializer.cpp`.

## Идентичность и ABI

Class ID `0x1C4C75BA`, C++ и registered base — direct `spSerializer`
(`0x42429877`), target — `spTextureData::ClassID` (`0x78EA082B`).

PS2 factory выделяет ровно `0x14` байт, вызывает общий constructor
`spSerializer` и меняет только два vptr. PC destructor аналогично возвращает
secondary vptr к таблице этого класса и переходит в base destructor; новых
полей состояния не наблюдается.

## Методы

PS2 secondary thunks `0x00178CC0/0x00178CB0/0x00178CA0` адаптируют write,
indexing и read. Index pass возвращает success без добавления relationships.

## Source-wrapper

Диагностические имена и оба call graph подтверждают поля:

| ID | Имя | Значение |
| ---: | --- | --- |
| 2 | `esfTextureDataSourceNone` | внешний source stream отсутствует |
| 3 | `esfTextureDataSourceEmbeded` | source — вложенный `spMemoryStream` |
| 4 | `esfTextureDataSourceReference` | source — ссылка на обычный `spStream` |

Если source отсутствует, writer пишет field 2 со значением `false`, но это не
означает пустую текстуру: вызывающий код продолжает записывать локальное
представление объекта. Reader зеркально различает `2/3/4`; reference-ветка
создаёт и открывает file stream относительно текущего ресурса, embedded-ветка
повторно вызывает serializer на текущем потоке, а None возвращает управление
локальному reader. Неизвестные поля безопасно пропускаются.

## Базовое локальное представление

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

## Открытые вопросы

1. Original header и имя secondary serializer interface.
2. PC factory42DC30 теперь исполнен, exact allocation14; полный rollback/failure ещё открыт.
3. Имена native mode и platform type enum.
4. Attached-source member/API, правила canonical reference path и resolver.
5. Точный status/rollback при ошибке вложенного либо внешнего потока.
6. Platform container mapping и производные DX/PS2 texture serializers.

Actual PC startup использует wire78EA082B для Data/DXData/PS2Data serializers
с разными platform masks; virtual target identifier нельзя использовать
как единственный registry key. Field3 вызывает **тот же selected serializer
read рекурсивно**, без нового8-byte object header:
[runtime/mips/registry](../../engine/textures/texture-runtime-mips.md).
