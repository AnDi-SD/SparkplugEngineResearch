# Importer: предпросмотр исходной модели через общий shader

Исходный SMO в позе подгонки раньше деформировался отдельным C# кодом
`TargetRigFittingPreviewBuilder`: cutoff малых весов, деление на сумму,
взвешенная матрица и отдельный расчёт normals. Identity pose вообще обходила
деформацию. Это расходилось с подтверждённым
[original Fixed.rfx](tool-gpu-skinning-shared-picking-2026-09-10.md).

CPU evaluator удалён. Core подготавливает authored fitting palette и передаёт
её вместе с неизменённым исходным mesh явному position-provider. Palette
composition вызывает существующий `SparkplugSkin.ComposeMatrix` → common
`spSkin`, а не C# копию перемножения. Входы уже в одной external coordinate
basis; общий helper не добавляет смену координат.

## Граница Core / platform host

В Importer не было своего OpenGL context. Добавлен GUI-only
`TargetRigFittingGpuPreview`: один скрытый context и существующий общий
`SmoGpuSceneRenderer.ReadPositionsForPicking`. Core остаётся без зависимости
от GL context или Windows; он требует provider, проверяет его output и
возвращает владеющий snapshot. Пустой/нечисловой output отклоняется.

Mesh geometry кэшируется по неизменным source references. Редактирование позы
меняет только palette и выполняет readback. Замена target/reset очищает кэш,
закрытие окна освобождает context; прежний GLFW/WGL context восстанавливается.
При отсутствии skinned meshes новый context не создаётся. В момент upload
проверяются finite weights и принадлежность активного joint реальной palette;
negative/tiny веса сохраняются. Неиспользованный byte index при weight0
не требует существующей palette entry. Предел этого backend —32 matrices.

Результат содержит **только positions**; normals пустые, не выдаются за
деформированные. Нынешний overlay caller использует positions и indices.
Прежняя UI диагностика отказа сохраняет явно обозначенный canonical gray
target. UI layout не менялся. Authoring fitting transforms, автоматический
оптимизатор, создание весов и inverse-bake donor не заменялись игровым runtime.
SAN playback и полная simulation позы подгонки не утверждаются.

## Проверка

`Viewer.GuiTests --importer-fitting-gpu Icy.smo OUTPUT` вызывает actual новый
Importer host; portable fixture используется через link, без копии numeric
assertions. Файл Icy509977B,3729vertices,12skinned meshes.

- **24 GPU/host checks PASS**: identity, root translation, local Spine_01
  rotation, fixed half/zero/negative/tiny weights, active invalid palette slot,
  допустимые unused slots, Clear/re-add, context restoration и Dispose.
- Три actual-target poses: один context, один scene upload,36 readbacks.
  Максимальная ошибка root translation `4.92288164e-5` при прежней границе
  `0.0025`; локальное движение11,6454. Исходные hierarchy, lengths, inverse
  binds, positions и файл не изменены.
- Весь GPU run:3,3985с, peak188600320B (179,9МиБ), RTX3070/OpenGL3.3.
  Числа включают дополнительные synthetic scenes и отдельный sentinel context;
  это не замер каждого интерактивного кадра или всей программы.
- Portable `--target-fitting-preview-regression Icy.smo` прошёл provider,
  ownership и invalid-output contract для12мешей. Этот mode сам skinning
  не вычисляет и не заменяет GPU проверку.

GUI и Core FormatTests собраны с0 warnings/errors. Первая сборка потребовала
обновить transitive OpenTK references после нового project reference; restore
выполнен из существующего локального NuGet cache. Затем linked fixture
потребовал explicit System.IO, поскольку implicit usings WPF отличаются.
Оба начальных failed build logs сохранены; исходники игры не подгонялись.

Проверка actual mouse interaction полного окна Importer, все модели и
неграфические authoring-операции не входят в этот адресный run.
Артефакты: `local-data/results/tools-core-cycle-20260910-0730/importer-fitting-gpu/`.
[Manifest](../../research/tools-core-importer-fitting-gpu-2026-09-10.json)
сохраняет hashes sources, DLL, inputs и результатов.
