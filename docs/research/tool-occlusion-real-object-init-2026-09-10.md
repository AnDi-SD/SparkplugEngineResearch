# OcclusionVolume: original Init и reader одного реального объекта

После [конечного разбора защищённой подготовки](tool-pc-protected-preparation-2026-09-10.md)
успешно исполнены original shape, полный Init и serializer для `oclusion_wall02`
из `Media/Levels/Challenges/race_02.smo`. Это снимает прежний VM blocker этого
объекта. Перенос его runtime в общий класс и whole-file загрузка уровня ещё
не подтверждены этими original-PC пробами.

Вход: resource ID 8, physical index 7, class `43D24430`, object extent
`[189978,190116)`, 138 байт. Две неизменённые секции после восьмибайтового
object header занимают 130 байт, SHA256
`0AEC16403AA90B884953D3C6FA542DE56AB0A677F0106EAA9BEF36DEDD5E2CFB`.
IndexBuffer содержит `[2,1,0,1,2,3]`; четыре position records побайтно различны.

| Операция | Инструкции | Время | Результат |
| --- | ---: | ---: | --- |
| Shape `470E30`, подготовленные owned buffers | 3 958 778 | 9,883 с | AL=1, 2 faces / 8 edges; initialized=0 |
| Init `470FE0`, buffers от actual CPU readers | 3 971 161 | 10,099 с | AL=1, собственные copies; initialized=1, camera-dirty=1 |
| Reader `44F400`, actual serializer factory `44F320` | 3 985 573 | 10,765 с | AL=1, cursor=130/130; сам вызывает Init и удаляет временные IB/VB |

Во всех трёх случаях отдельный свежий guest: существующий профиль
`character` 4M/16s выбран до создания, процесс 30s, один worker, heap 64KiB.
Ни stopped/resumed guest, ни подмена защищённого producer не применялись.
После штатного возврата actual destructors освободили все отслеживаемые
выделения. Bump usage: shape 53 888, Init 54 160, reader 54 256 байт;
это не измерение всей host RSS.

В Init/reader явно использован прежний bounded CRT insertion-sort fixture,
каждый comparison которого вызывает original `4607F0`. Это не восстановленный
MSVCRT qsort. На данном входе все ключи различны; original weld не выполняет
duplicate remap/compaction. Неопределённый выбор representative при ties
по-прежнему не закрыт для других входов.

Original результат: borderCount=4, planar=1, две faces и восемь edges.
Сфера `(0, 284.1669616699219, -1002.2327880859375, 5263.40869140625)`.
Min `(-4958.6494140625,-82.49378204345703,-2728.739990234375)`,
max `(4958.6494140625,650.8276977539062,724.2744750976562)`.
Порядок рёбер, face position pointers, float bits и outgoing lists сохранены
в captures; одно лишь равенство числа элементов не считается сравнением topology.

Дополнительное наблюдение reader замкнуло cursor producer: после helper
`856CF0` на `13B4F8E` EAX=`FFFFBF46`, stack word=`31010FEE`.
Их сумма modulo32 равна `3100CF34`; EAX отдельно **не равен четырём**.
На `13B4FA8` cursors `3100CF34/3100CF3A` дали исходные тройки `[2,1,0]`
и `[1,2,3]`. Защищённое преобразование сохранённого указателя не выдаётся
за дополнительное правило формата IndexBuffer.

Serializer factory действительно выделил 20 байт и установил `6E62EC`.
Reader исполнил inherited Node fields и собственные buffer fields, но здесь
нет FAT materialization, загрузки других 7512 объектов или сцены игры.
Original Init одного входа также не доказывает re-init lifetime и все failure paths.

Локальные probes/captures:
`local-data/results/tools-core-cycle-20260910-0730/occlusion-init-next/`.
[Manifest](../../research/tools-core-occlusion-real-object-init-2026-09-10.json)
содержит fingerprints и конкретные ограничения. Исходники игры и native DLL
для этих наблюдений не менялись; сборки и GPU не запускались.
