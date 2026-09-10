# MaterialColor через живой граф инструментов

После подключения [PC factory](tool-pc-material-color-factory-2026-09-10.md)
существующий `SparkplugMaterialRuntime` проверен на реальном
`Media/Characters/Knut/lightbeam_projectile.smo`. Нового reader, evaluator,
планировщика кадра или межъязыкового API не добавлено.

Прежний regression предполагал, что все выбранные материалы не имеют color
controller. Он теперь проверяет фактическую ссылку и pending clock, учитывая
повторное использование controller разными материалами. Дополнительно проверены
кеш кадра, `force`, потребление времени ровно один раз и отсутствие вычисления
без нового времени. Формулы остаются в общем восстановленном классе.

| Реальный вход | Материалы / controllers | Проверки | Время процесса |
| --- | ---: | ---: | ---: |
| Knut/lightbeam_projectile | 1 / 1 | 23 PASS | 1.77 с |
| Icy/Icy | 3 / 0 | 23 PASS | 0.39 с |

Lightbeam полностью загружается: material ID 5 связан с controller ID 6.
После `Apply(.125)` первый `UpdateColor(frame=5)` потребляет время до `.125`.
Новые `.25` остаются pending при том же frame; `force` потребляет их до `.375`.
Повторный force и новый frame без elapsed не вызывают evaluator.
Managed scene уничтожен; обращения через оставшуюся runtime-ссылку отклонены.

У этого контроллера все пять function types равны 0. Четыре цвета закономерно
сохраняются, включая diffuse alpha `0.49803925`. Это проверка живого графа,
часов и кеша кадра, **не демонстрация меняющегося цвета или готового renderer**.
Shared-controller alias допустим в проверочном коде, но у двух выбранных файлов
такой случай отсутствует и новым acceptance не считается.

Предыдущий original PC corpus probe использовал те же 46 байт section, но
явно подготовленный controller и отдельный actual Material с diffuse alpha
`.375`. Он проверял reader/update/writer на этих входах, а не данный whole-file
graph. Исторические bounded reports от 6–7 сентября не подменяют новый
acceptance. Полная original factory доказана отдельным блоком 10 сентября.

Сборка `SmoViewer.FormatTests` Release с `--no-restore`,
`-p:SkipSparkplugNativeBuild=true`, `-m:1`: 25.06 с, 0 warnings/errors.
Использована проверенная DLL
`939B42687CE8806F910334D17B19AB3917769F08A867D040F6041B53A3456D97`.
Каждый acceptance — отдельный процесс с внешним пределом 30 с.

[Lightbeam report](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/color-runtime-lightbeam/report.json),
[Icy report](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/color-runtime-icy/report.json),
[build log](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/color-runtime-managed-build.log).
Точные hashes исходников и результатов сохранены в
[manifest](../../research/tools-core-material-color-live-graph-2026-09-10.json).

Автоматический scheduler, animation manager gate, multipass rendering,
визуальное воспроизведение и whole-file исполнение оригинальной игрой этим
блоком не проверялись. Production код после factory не менялся.
