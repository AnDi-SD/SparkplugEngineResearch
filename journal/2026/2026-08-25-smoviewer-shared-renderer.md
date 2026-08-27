# SmoViewer: общий WPF/OpenGL renderer

Дата: 2026-08-25.

## Причина

GPU renderer SmoViewer находился внутри partial-класса `MainWindow`. Его нельзя
было подключить к SmoLVLcreator без копирования реализации и последующего
расхождения исправлений между программами.

## Новая граница

Созданы два общих проекта:

- `SmoGpuSceneRenderer` владеет OpenGL 3.3 ресурсами, shader path, MSAA,
  прозрачными проходами, skinning, texture animation и floor grid;
- `SmoViewer.Scene` и `SmoSceneBuilder` владеют единым decode-to-render pipeline;
- `SmoSceneMesh` является публичным подтверждённым render-input;
- `SmoRenderObjectKey` отделяет renderer identity от GUI selection-моделей;
- `SmoViewportMath` содержит общую математику viewport, используемую также WPF
  fallback, и перевод WPF camera/cursor в native SMO picking ray;
- `SmoScenePickIndex` кэширует picking bounds и выполняет точное пересечение с
  треугольниками, включая исходную skinned-позу.

`SmoViewer/MainWindow.GpuRenderer.cs` теперь является только host-адаптером: он
создаёт GL context и передаёт camera/window state. Реализации GPU renderer и
подготовки сцены в GUI больше нет.

SmoLVLcreator ссылается на обе DLL, передаёт подготовленную сцену тому же
рендереру и владеет только editor-specific камерой и выделением. Каталог моделей
строится по той же `SmoPreparedScene`, поэтому renderer и inspector не могут
разойтись в трактовке материалов, размещений или shared instances.

## Проверка

- `SmoViewer.FormatTests`: 174 assertions;
- `SmoLVLcreator.CoreTests`: 78 assertions;
- Debug и Release сборки Viewer и SmoLVLcreator завершены без ошибок;
- GPU smoke-test Viewer `fish.smo`: 1 unique mesh buffer, 1 unique texture,
  1 placement; texture, skinning, depth и floor grid визуально сохранены;
- скрытый runtime smoke-test SmoLVLcreator с `fish.smo` прошёл без падения после
  создания OpenGL context и загрузки сцены;
- автоматический ЛКМ smoke-test выбрал `fish` и вернул точное попадание в
  triangle 15, после чего GUI синхронизировал placement и inspector.
- editor Move smoke-test отобразил world-space gizmo в центре entity, сдвинул
  `fish` по X, обновил inspector и dirty marker;
- transform-session tests cover grouped mesh parts, multi-entity preview,
  atomic commit/undo, and exact cancel without a history entry.

Предупреждения `NU1900` во время сборки относились только к недоступному сетевому
NuGet vulnerability index; зависимости были восстановлены из локального cache.

## Связанные коллизии в SmoLVLcreator

Редактор сохраняет visual placement и collision shape раздельными сущностями,
но `SmoLVLcreator.Core` теперь строит мягкие `SmoLevelCollisionLink` для
высокоуверенных совпадений world-space bounds внутри одного `spPartitionNode`.
GUI по умолчанию включает `MOVE LINKED`: gizmo и числовой position editor
передают обе сущности в одну transform-session, поэтому commit/undo/save остаются
атомарными. Переключатель можно отключить для независимого перемещения.

На исходном `Alfea02_old.smo` подтверждена пара table visual `[2722]` и
`spCollisionInfo [1328]`. Интеграционный тест переносит их вместе, сохраняет
render matrix и collision vertices, повторно открывает SMO и проверяет обе
world-space позиции; отдельный collision-only preview также проверен.

## Rotate и Scale

Transform-session в `SmoLVLcreator.Core` поддерживает preview вращения вокруг
world-axis/pivot и ориентированного масштаба. Editor viewport получил три
локальные/world rotation ring, осевые scale handles и uniform center handle.
`Ctrl` включает шаг 15° для Rotate и 0.1 для Scale. Операции используют общий
pivot и world-delta, поэтому multi-selection и `MOVE LINKED` не расходятся.

Collision writer теперь запекает полную affine-delta в локальные вершины
`spMeshBV`, сохраняя собственный `spCollisionInfoTransform`. Round-trip тесты на
паре Alfea table `[2722]` / collider `[1328]` подтверждают rotation и non-uniform
scale после сохранения и повторного открытия SMO.

## Multi-selection export и файловое меню

Верхняя панель SmoLVLcreator освобождена от Open/Save/Save As. Настоящее меню
`Файл` содержит открытие, сохранение, быстрый экспорт и `Экспортировать как…`;
Undo/Redo и переключатели панелей вынесены в `Правка` и `Вид`. Создано отдельное
окно `Инструменты > Настройки…` с профилем каталога и формата быстрого экспорта.

`SmoExporter.Core` получил `SmoExportSceneSelection`: несколько конкретных
placement с текущими editor world matrices собираются в одну GLB/FBX/OBJ scene.
`SmoLevelExportService` является тонким адаптером mutable selection к общему
экспортному ядру. Интеграционный тест выбирает три Alfea entities, изменяет их без
сохранения SMO, проверяет все mesh parts и записывает один GLB во временный каталог.

## Составные модели уровня

В `Alfea02.smo` авторская модель может быть разложена на несколько соседних
`spStaticRenderObject`, а не храниться одной веткой. Например, меши `[2125]`,
`[2129]` и `[2133]` принадлежат отдельным владельцам `[2122]`, `[2126]`, `[2130]`,
но имеют общее имя `tree01`, модели `tree01-000/001/002`, один `spPartitionNode`
`[2090]` и полностью одинаковую world matrix.

`SmoLVLcreator.Core` теперь консервативно восстанавливает такие
`SmoCompositeModel`: одновременно должны совпасть непустое имя владельца,
serializer-parent и точная матрица, а имена моделей обязаны иметь уникальные
трёхзначные суффиксы `-000`, `-001`, ... . В текущем уровне подтверждены 83
размещения (212 физических частей), образующие 46 составных ресурсов.

Каталог моделей показывает один собранный ресурс и строит общее превью через
`SmoViewer.Rendering.Wpf`. Обычный клик по любой части выделяет и перемещает весь
набор, `Alt+ЛКМ` оставляет доступ к отдельному физическому мешу. Исходные entities
не сливаются: сохранение и экспорт получают все отдельные объекты, а низкоуровневое
независимое редактирование остаётся возможным.

Второй подтверждённый layout — геометрия, запечённая непосредственно под
`spPartitionRenderable`, без `spStaticRenderObject`. Так хранится `Muza_bed`:
каркас `[1691, 1694]`, верх `[1706, 1709]` и навес `[1697]` записаны как
`Muza_bedNOSH`, `Muza_bed01` и `Muza_roofbed`, но пространственно и по соглашению
имён образуют одну модель. Ядро объединяет пронумерованные части с одинаковым
authored name, а для подтверждённого семейства `bed` также нормализует `NOSH`,
номер варианта и `roofbed`.
