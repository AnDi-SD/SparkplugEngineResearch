# Legacy TextureData: граница source-wrapper

10 сентября 2026. Статус: **не закрытая runtime-загрузка**. Direct-field preview
остаётся явно обозначенным чтением сохранённых данных; перенос его C#
source-оболочки в общий игровой reader пока не выполнен. Это не основание
добавлять в восстановленный reader необязательный Source wrapper.

## Реальный файл и выбор serializer

`local-data/pc-pristine/Media/SFX/book.smo`, 68604 байта,
SHA-256 `F8FC402E787A7FF1A7426A403074C40A454131886E931AC423C2A1742E168932`.
FFPS: version38 (`0x26`), exportTag41, platformMask1, dataOffset420.
TextureData: index11, resource ID12, object offset1330; payload `[1338,66902)`,
65564 байта, SHA-256
`7BC4DEA5E6CB7C866DCA014E51C559678C22E6EFD6F70800E0445255A299E6B3`.
Все offsets здесь физические, границы полуоткрытые.

[CP15 registry](native-pc-texture-runtime-mips.md) уже исполнил шесть actual
startup registrations и original lookup4224F0. Для wire `78EA082B`, mask1,
load1 выбран **spTextureDataSerializer**, factory42DC30/vtable6DDD90.
Это не DXData42C640, выбираемый для mask2/4. Свежий статический просмотр
подтверждает initializer6D1940: `(operation=1, platform=1, factory42DC30,
wire78EA082B)` передаются registration422D90.

Primary vtable slot1C содержит42DD10: читает восемь байт object header без
проверки обоих слов и создаёт runtime DXTexture через4AB520. Secondary vtable
6DDD84 slot8 содержит42F180. Следовательно, header factory не выбирает
другой payload reader. Это согласуется с [общим inline-load протоколом](native-pc-read-reference.md).

[Manager validator](native-class-sp-serializer-manager.md)422260 проверяет
signature, version26, stream size, mask&3 и dataOffset; exportTag не проверяет.
Generic loader422C0E..422C20 переносит header.platformMask в manager+10,
устанавливает operation1. В просмотренных validator/dispatch/texture-reader
ветвях выбора reader по exportTag41 или необязательного wrapper не найдено.
Это ограниченный вывод об этих методах, не поиск всех compatibility paths игры.

## Что делает выбранный оригинальный reader

Свежий read-only disassembly pristine PC EXE
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`:

| Адрес | Наблюдаемая инструкция / ветвь |
|---|---|
|42F1B6|Безусловный call42EA50 до локального field loop.|
|42EABB..42EACC|Source wrapper различает только field2/3/4.|
|42EACE..42EAD4|Другой ID, включая direct field0, идёт в Skip472AC0.|
|42EC6A..42ECA3|NULL header или terminator завершает wrapper с AL1.|
|42F227..42F243|При handled=false запускается следующий local field loop.|
|42F25B..42F2D9|Только local field0 вызывает cross reader42E100; прочие поля пропускаются.|
|42F248/42F255 →42F2F4 →42F331|NULL header/terminator допускает AL1 без вызова cross reader и без Init.|

У book direct field0 имеет header `[1338,1343)`, payload65558 байт до66901;
следующий байт — terminator. Таким образом, Source wrapper пропускает весь
cross block и заканчивается на66902. Он не переинтерпретирует field0 как
локальные pixels. Если после него доступен только ограниченный object slice,
следующий original ReadHeader получает EOF: статическая ветвь допускает bool
success без initialized texture.

В полном файле после66902 лежат родительские field2(size20), field6@66924(size62)
и terminator66991. Original reader не получает byteCount и по просмотренному
control flow может потребить их как собственную локальную секцию. Это
**статический риск выхода за object boundary**, не исполненная whole-book
загрузка. Ни принятие book игрой, ни её окончательная реакция здесь не доказаны.

## Граница инструментов и следующий шаг

Общий host reader намеренно ограничивает object/section extent и требует
действительно прочитанных pixels. Пустая локальная секция не превращается в
успешно инициализированную текстуру. Новая PC source inspection исполняет этот
reader; для native-PC пути дополнительно требует PC mask bit2 и полный trace.
Оригинальное AL1 без Init не является основанием ослаблять эту гарантию.

Для book `SmoTextureDataDecoder` оставляет `LegacyCrossPlatform`,
`UsesRuntimeSourceSelection=false`, `RuntimeMipLevelCount=null` и ограничение
редактирования. Leaf `TryParseCrossPlatform` уже вызывает общий
`spTextureDataSerializer::ReadCrossSectionForAnalysis` через
[существующий bridge](tool-texture-sections-shared-core-2026-09-09.md).
Превью сохранённых raw pixels не выдаётся за результат игрового source dispatch.
Оставшаяся C# проверка direct-field source shape — открытая граница миграции.

Следующий необходимый контракт, если потребуется именно runtime-поддержка
этого legacy asset: подтвердить совместимость выбранного42F180 с direct-field
формой и реальное состояние созданного42DD10 объекта в её настоящем caller.
До такого evidence не добавлять fallback/optional wrapper и не объявлять файл
неподдерживаемым игрой только по bounded host failure. Отдельная общая HOST
legacy metadata-оболочка была бы предложением по правилу2, а не восстановленным
original reader; её реализация здесь не согласовывается и не выполняется.

В этом проходе выполнено только чтение файла и статический disassembly. Guest,
whole game, чтение родительских полей оригинальным кодом и увеличение micro
caps не запускались. Машиночитаемые offsets, branches и границы:
[evidence JSON](../../research/tools-core-legacy-texture-boundary-2026-09-10.json).
