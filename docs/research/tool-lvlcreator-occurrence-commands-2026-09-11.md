# LVLcreator: команды по фактическим размещениям — 11 сентября 2026

Блок 6 цикла до 19:00. LVLcreator больше не отказывает при создании
редактируемого документа с повторными ссылками на Model. Общий native graph
и игровые алгоритмы не менялись: исправлена адресация в потребителе.

## Изменение

`SmoPlacementId` содержит прежние Mesh/Scene indices и `SmoRenderOccurrenceKey`
(реальный container/member slot). Все слоты сохраняются в словаре; старый ключ
из двух индексов принимается только при единственном совпадении. При нескольких
совпадениях возвращается явная неоднозначность, первый слот не выбирается молча.
Отдельный индекс совместимости строится один раз, без линейного поиска на pick.

Transform entity определяется actual RenderNode/StaticRenderObject container
из общего `SmoLoadedResources`, с проверкой соответствия member slot. Физический
родитель места хранения Model больше не выбирается вместо его actual support.
У нескольких ссылок одного контейнера остаётся **одна игровая трансформация**:
слоты раздельные, движение их узла общее. Придумывать независимую трансформацию
для каждого слота или создавать скрытые дополнительные Node не требуется.
Другие container kinds сохраняют прежнюю ограниченную authoring policy.

Ключ передаётся через collision bounds, composite grouping, selection export и
диагностику сохранения. Save job использует реальный object index transform owner;
формат job менять не потребовалось. В GUI сделаны только подключения ключа:
catalog, selection, picking и camera focus, без перестройки интерфейса.

## Проверки

- `Media/Menus/igmenu_opt_pc.smo`: **207 editable placements**, повторные slots
  больше не останавливают constructor, 1149 checks exact identity/owner.
- `Media/Levels/Alfea/Alfea02.smo`: **1008 placements**, 5947 checks; прежний
  неповторный файл сохраняет все размещения и actual container ownership.
- На menu выбран mesh index57 / Model54 / container53 slot0 (игровые IDs
  на единицу больше). Этот RenderNode содержит два одинаковых Model slots.
  Translation меняет общий узел, остальные размещения не сдвигаются.
  Проверены undo/redo, exact selection export, запись isolated save job,
  его Execute и повторная загрузка всех 207 размещений. **423 checks**,
  1,834 s, patched owner1, max world error **2,3841858e-7**; исходник не изменён.
- Прежние workspace/editor tests `fish.smo`/`bloom_ball.smo`: **63 assertions**,
  включая history, sessions, отмену, rotation/scale. CoreTests и GUI собраны:
  0 warnings/errors. Визуальный запуск GUI этим блоком не заявляется.

Первый edit test ошибочно искал повторения одного Model в разных контейнерах;
в menu повторения находятся внутри одного RenderNode. После проверки actual
slots критерий теста исправлен; поведение игры и production code не подгонялись.
Два начальных запуска остановились до редактирования/сохранения.
Исправлены также nullable annotation и две ошибки компиляции новых подключений.

Старый общий workspace test ожидал непустой CPU picking index у Skin без
backend positions. Отказ происходит до нового `SmoLevelDocument`: он относится
к прежнему переходу на общий GPU skinning. Проверка теперь требует, чтобы каждый
слот либо участвовал в picking, либо имел явный `SKIN_PICKING_POSITIONS_UNAVAILABLE`.
Она не создаёт подставные skinned vertices. Исходный failed log сохранён.

## Границы

Source game logic и serializer algorithms не менялись, повторный запуск EXE
не проводился. Доказательства actual slots переиспользованы из
[общего scene](tool-render-occurrences-shared-core-2026-09-09.md) и
[Exporter](tool-export-occurrences-shared-core-2026-09-09.md); сохранённый output
проверен общим восстановленным loader. Это не новая оценка изученности PC/PS2.

Разделение transform одного контейнера для независимого движения его member
потребует явной операции изменения графа. Missing position fields, прежние
inverse-world/nonuniform и legacy static inverse policies остаются отдельными
границами authoring; текущий блок не объявляет их исправленными.

Manifest: `research/tools-core-occurrence-commands-2026-09-11.json`.
Raw/snapshots: `local-data/results/tools-core-cycle-20260911-1900/placement/`.
