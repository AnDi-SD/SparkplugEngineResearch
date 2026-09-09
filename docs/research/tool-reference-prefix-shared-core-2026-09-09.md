# Общий префикс ссылок ресурсов

Двенадцатый блок цикла. C# `SmoNodeDecoder` и `SmoMaterialDataDecoder` удалили
собственный разбор ID/inline size и inline object header. Общий helper
`spSerializer::ReadReferencePrefixForAnalysis` извлечён из существующего
runtime resolver; его используют также прежние guards отдельного поля и
последовательности ссылок. Полная загрузка по FAT/cache остаётся прежней.

## Что исправлено в инструментах

Оригинальные PC `4678B0/467670` читают ID, при ненулевом ID — size из второго
stream. Null потребляет только четыре байта. Старый C# считал ненулевой
четырёхбайтовый ID отдельным legacy encoding. Это не контракт общего reader.
Свежий failed-size probe доходит до `467698` после неуспешного Read; оригинал
не проверяет его результат. Это контролируемая остановка, **не** завершённая
загрузка повреждённой ссылки и не доказательство штатного отказа игры.
Прежний общий host resolver уже отвергал такой ввод; теперь инспектор делает
то же. Восстановленный игровой алгоритм не подгонялся под старые тесты.

Тесты Node/RenderNode и компактного Model использовали выдуманную short nonnull
форму; переведены на ID,size0. Компактность Model по-прежнему проверяется
отсутствием необязательных scalar fields. BSP fixture передавал short nonnull
Zone, хотя историческое досье подтверждало в корпусе только null. Он теперь
проверяет null, как реальные BSP. Иной BSP Zone encoding этим блоком не
подтверждён и не объявляется поддержанным.

В существующей read-only базе на выбранных PC menu, PS2 menu, Bloom jeans
и tile_bad коротких ненулевых ссылок Node/RenderNode/Model/Renderable не найдено.
Это ограниченная выборка, не новая проверка всего корпуса. Имена enum
`IdOnly`/`LegacyIdOnly` оставлены ради совместимости DTO; второй больше не
выдаётся новым material reader. Наличие enum не означает принятие формы.

## Граница inspector ABI

`SpvReferencePrefix` содержит четыре UInt32: ID, inlineSize, encoding, classID.
`spv_reference_prefix` kind0 читает сохранённый prefix с объявленным полным
размером; kind1 получает весь payload и читает inline header общим reader.
Stream borrowed, вход не копируется, объектов/менеджеров не создаётся.

Совпадение объявленного extent, минимальный inline header и canonical SBOO —
host guards инспектора. Original header factory не обязан проверять marker;
runtime ReadReference также имеет более опасные malformed ветви. Наличие
metadata не утверждает успешного RTTI/factory/relationship разрешения.
C# сохраняет каталог, поиск уникального ID и class-specific ограничения
оставшихся typed decoders. Эти decoders ещё требуют переноса.

## Проверка

`research/validate_tools_reference_prefix.py`: три свежих original пути
(null, заранее созданный объект, полное inline materialization), отдельный
split-stream сценарий и controlled failed-size stop. Original fixtures
завершают свою проверку освобождения. ABI metadata совпали; retained prefix
и пять malformed host refusals проверены отдельно.

`ReadReference`:266 C++ checks. Четыре Viewer FormatTests набора и три whole
resource graphs прошли; точные counts, входы и SHA256 в snapshot блока.
Whole graphs: PC menu1225 objects/835 nodes, tile_bad127/35, Bloom121/98.
Это проверка текущего общего loader и соответствующего FAT evidence;
не новое исполнение этих трёх файлов в игре и не PS2 runtime.

Документация исторических sealed checkpoints остаётся неизменной. Этот блок
уточняет прежние утверждения инструментов о legacy references. Общий Node
scalar decode и другие typed resource данные остаются следующим приоритетом.
