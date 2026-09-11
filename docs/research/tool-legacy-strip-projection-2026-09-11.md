# Общая проекция legacy strips, 11 сентября 2026

Блок13 цикла до19:00. Исправлена host-причина, из-за которой все41 Mesh
`Media/Menus/menu.smo` отклонялись после успешной загрузки общего graph.
Игровые исходники не исправлялись: original reader сохранял ровно то, что
хранится в файле. Вся необходимая поправка относится к современному backend.

## Доказанная причина

У каждого из41 strips два последних UInt16 имеют значение`CDCD`. Остальные
индексы находятся внутри VB. Хвост участвует только в вырожденных треугольниках:
повторяющиеся значения делают их пустыми. Прежний host проверял все raw
индексы до преобразования, хотя его C# strip converter эти окна отбрасывал.

Fresh original PC probe выполнил factory42AEF0/42AFD0, reader42B420,
IB reader45FB80 и DX initializer4AA000 на настоящих Mesh ID6/11 из SMO.
Оригинал принял оба объекта, потребил202/750 bytes и скопировал index/vertex
bytes без изменений, включая`CDCD`. Обычный teardown освободил все tracked
allocations; COM/stream/declaration services заданы fixture, GPU не исполнялся.
17 checks,2,684s; arena63904/64976 bytes. Это доказательство reader/upload,
не утверждение о поведении реального D3D при выводе такого хвоста.

## Изменение приложений

`Sparkplug/Analysis/Host/RenderTopology.h` — одна общая host-проекция list/strip
в triangle list. Raw indices остаются отдельным неизменным массивом. Strip
degenerates пропускаются **до** проверки диапазона; parity продвигается для
каждого исходного окна. Каждый индекс выданного треугольника проверяется по VB.
List-поведение, включая сохранение допустимых degenerates, осталось прежним.
Настоящий invalid nondegenerate triangle по-прежнему отклоняется.

`spv_mesh_triangles` предоставляет count/query/copy; выход ограничен64MiB.
Прежний `spv_mesh_indices` возвращает raw source indices: его потребитель
не должен отправлять неподтверждённые raw значения в свой backend. Metadata
view и неподдержанные primitive types не имеют triangle projection.
Из `SmoMeshDecoder` удалены обе C# conversion routines; все инструменты,
использующие общий decoder, получают один native результат.

## Проверки и границы

Raw: `local-data/results/tools-core-cycle-20260911-1900/menu-mesh/`.
Манифест: `research/tools-core-legacy-strip-projection-2026-09-11.json`.

- Native:12 topology checks +346 существующих MeshReader checks,2/2 suites.
- C ABI:373 checks,41 Mesh/1284 vertices/1202 emitted triangles,0,0325s.
  Raw bytes сверены с исходником и двумя original captures; проверены short
  output canaries и намеренно повреждённый nondegenerate triangle.
- Managed menu:124 checks,41 meshes,0,584s,45,8MiB peak. Десять ошибок
  неподдержанного Text GPU сохранены; ошибок Mesh больше нет.
- RTX3070: menu41 placements/41 passes,23160 pixels192×192; live0,61–1,66ms,
  peak181,4MiB. PNG просмотрен: видны фон и десять кнопок, текста ещё нет.
- Контроль Icy12 placements и Alfea021008 placements прошёл; общий material,
  shader-lighting и alpha consumer принимает новую проекцию. Средние live
  frames0,28/6,46ms на небольшом192×192 target, не игровой benchmark.
- Native/GuiTests/FormatTests сборки прошли без ошибок.

Начальная диагностическая утилита ошибочно считала graph-relative offset
физическим; assert остановил чтение до каких-либо изменений. Исправлено через
общий container header. При добавлении suite восстановлены исходные LF/BOM
настройки Build-Native после ненужного изменения кодировки скриптом.

Menu GPU Text, legacy texture source-selection и полный игровой frame ещё
открыты. Новый код — backend projection, не восстановленный D3D draw. PS2
geometry runtime этим не подтверждается. Общий EXE score не увеличивался.
В прежних Font/Text dossiers также исправлены устаревший atlas type и явная
граница исторического wire-среза; сами original captures сохранены.
