# Перенос SMO forest по настоящим ссылкам

11 сентября 2026, блок 18. Native SMO → SMO replacement в Importer больше
не распознаёт ссылки по восьмибайтовому размеру поля. Он использует записи
общего игрового reader из `SmoFileReferenceTrace`, включая ссылки внутри
Skin palette и AnimTexController. Восстановленные алгоритмы не менялись.

## Исправленная ошибка

Контрольный перенос StellaX в Icy старым кодом создал файл с правильным FAT
и точными Mesh/Texture payloads, но пропустил **29 ссылок** в массиве текстур
`spAnimTexController` (класс `16FB0E47`). Старый файл имеет SHA256
`24F2E1E884B1C9E09E3F54CAA00F5B851A92D149FBECF29321F188F799FA5D2B`.
Общий reader отклоняет его с `Reference ID is absent from FAT`.

Новый результат имеет тот же размер 653891 байт и те же 122 FAT entries.
Отличаются только 29 ID sites внутри контроллера: например 77 → 195.
SHA256 результата:
`172A9EA7D698A8A96050684A954DF42D6842BEC4DB9A13569788B6CDCA133FA1`.
Загрузка проходит, все 38 ключей анимации материала и runtime mip pixels
совпадают с донором. У исходного и нового каталогов разные ID; сами texture
и mesh payloads не перекодируются.

## Реализация и ускорение

- Общая `SmoFileReferenceRange.Relocate` проверяет hash исходных bytes,
  наблюдённые позиции, наличие назначений и уникальность копируемых объектов.
  Host выбирает межфайловые identity и заранее выделяет свободные ID.
  Неиспользуемые ID исходного каталога не резервируют ID другого файла.
- Retained target fixups выполняются по настоящим reference-only sites
  непереносимых consumers до удаления старых веток. Их inline IDs сохраняются
  до удаления. External donor links сопоставляются по class и точному RawName.
- Inline forest ranges фиксируются одним batch, затем общий перенос меняет
  все записанные reader ID, включая вложенные массивы. Отдельный обход Skin
  palette для перенумерации удалён. Финальный файл повторно проверяется общим
  reader перед установкой результата, включая временный файл на диске.
- Ветки удаляются существующим `RemoveInlineBranches`, а последовательная
  выдача ID выше максимального ID исходного target больше не сканирует каталог
  на каждом объекте. Неподтверждённого коэффициента ускорения не заявляем.

Нормализация donor-only bones сохраняет прежнюю явную host policy: ближайший
общий ancestor и его inverse bind. Прямые ссылки здесь также определяются
по reader sites; Node child определяется подтверждённой schema, opaque fields
не удаляются. Palette ID и inverse bind меняются вместе отдельной typed
операцией. Обычная перенумерация не меняет её матрицы.

## Проверка

Пакет ограничен двумя персонажами и маленькой направленной fixture.

| Направление | Mesh | TextureData | Materials / passes / layers | Animation keys |
|---|---:|---:|---:|---:|
| StellaX → Icy | 8 | 12 | 5 / 6 / 6 | 38 |
| Icy → StellaX | 12 | 1 | 3 / 3 / 3 | 0 |

Для каждого материала сравниваются все render states, colors, power,
controllers, passes, layers, UV matrices, texture states, времена ключей,
identity текстур и hashes каждого настоящего runtime mip level. 26 проверок
сравнения и две полные проверки сохранения target nodes/leaf payloads прошли.
В обратном направлении нормализованы Hair_01…04 и Cape_01…02.

73 проверки общего remap/relocation включают вложенные palette references,
retained consumer, одинаковые ID в разных каталогах, запрет коллизий/нулевого
назначения/неполного каталога, изменение source bytes и opaque eight-byte
fields внутри копируемого и retained объекта. Все входные files и owned
arrays остаются неизменными. Последние отдельные операции Replace заняли
0,179 и 0,339 с после Analyze; это не сопоставимые с холодным baseline замеры.

## Найденные промежуточные сбои и границы

Первая тестовая fixture ошибочно задавала Node двух родителей; игровой
reader её отклонил. Исправлена fixture: физический owner находится в palette,
единственный Node parent остаётся в target.

При добавлении trace в нормализацию первый вариант проверял промежуточный
palette после замены ID, но до переноса inline definitions. Это временно
создавало forward references и закономерно не загружалось. Reader observations
перенесены до этой стадии; проверяется завершённый результат. Один старый
test CLI без catch задержался на обработке необработанного исключения и был
остановлен по 30-секундному пределу; соответствующие CLI теперь возвращают
ошибку через catch. Неудачные результаты сохранены отдельно.

Это операция прямого переноса уже сериализованных PC SMO. Flat ImportedScene
и его preview всё ещё не выражают полный multipass material; их прежний
`MATERIAL_IMPORT_SHAPE` guard не снят. Общий reader coverage остаётся условием
переноса. Отдельного original execution и PS2 runtime в этом host блоке нет;
изученность EXE не увеличивается за исправление приложения.

[Manifest](../../research/tools-core-native-visual-transfer-2026-09-11.json)
связывает выбранные sources, binaries, checks и raw outputs в
`local-data/results/tools-core-cycle-20260911-1900/donor-transfer/`.
