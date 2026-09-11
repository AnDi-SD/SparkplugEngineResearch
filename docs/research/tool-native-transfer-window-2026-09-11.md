# Importer: SMO material shape больше не блокирует native transfer

11 сентября 2026, блок 20. Настоящее окно Importer теперь загружает StellaX
и позволяет построить план native замены Icy. До изменения загрузка донорского
предпросмотра завершалась `MATERIAL_IMPORT_SHAPE`, хотя
[ядро полного переноса](tool-native-visual-transfer-2026-09-11.md) уже сохраняло
все passes, layers, textures и AnimTexController references.

## Минимальное исправление подключения

Окно уже отключало final textured preview для SMO-доноров и рисовало только
геометрию. Однако loader прежде требовал full `ImportedScene`, который не
может выразить многопроходные материалы. Новый
`SmoModelReader.ReadNativeTransferGeometryPreview` использует тот же общий
SMO graph/экспортный geometry adapter, выбирая только Meshes и Skeleton.
Материалы не преобразуются, PNG для этого geometry preview не создаются.
Возвращаются прежние позиции, индексы, UV, skeleton mapping и веса; поле
MaterialIndex равно -1, каталоги ImportedMaterial/ImportedTexture пусты.

Это явно названная host projection для существующего geometry-only окна.
Она не является сериализатором и не обещает игровой textured frame. UI
получил одну развилку loader и точный текст области предпросмотра. Core Save
по-прежнему принимает оригинальные target/donor `SmoDocument`, а не preview.
Обычный `Read`/`ReadGeometryOnly` для внешнего authoring сохраняет прежний
material-shape guard: небезопасное уплощение не разрешено этим изменением.
Редкие ограничения преобразования skeleton в preview остаются отдельно.

## Проверка фактического окна

Оба запуска используют настоящий WPF MainWindow, target loader, асинхронный
donor loader, обработчик кнопки построения плана и состояние Save. Окно
не показывается. Запись вызывается той же core операцией без file picker.

| Донор → цель | Preview meshes/vertices | Native materials | Checks | Время внутри теста |
|---|---:|---:|---:|---:|
| StellaX → Icy | 8 / 3189 | 5 | 13 | 1,56 s |
| Icy → StellaX | 12 / 3729 | 3 | 15 | 1,71 s |

Результаты обоих переносов побайтно совпали с qualified output блока18:
`172A9EA7D698A8A96050684A954DF42D6842BEC4DB9A13569788B6CDCA133FA1`
и `8CB9846A0549C40130073ABFEBD7CBB4A08CFA977EEDEC24E3B2E6F8B6512141`.
Whole output reader прошёл. Для обычного Icy геометрия, индексы, оба UV,
skin weights, slots, имена и inverse binds отдельно совпали с прежним full
reader. Входные файлы не изменены. Пики процессов — 121,45/122,46 MB.

Первый тест ошибочно ожидал enabled Save сразу после загрузки; штатный
workflow требует нажатия «Проверить нативную замену». Исправлен тест:
он вызывает настоящий `Plan_Click`, не снимает guard и не подставляет план.
Производственное поведение кнопки не менялось.

Managed сборка GuiTests/Importer завершилась без warnings/errors.
[Manifest](../../research/tools-core-native-transfer-window-2026-09-11.json)
связывает выбранные исходники, binaries, raw reports и оба output.
Оригинальный EXE в этом integration блоке не исполнялся; PC/PS2 scores
не повышаются. Полный native textured preview остаётся UI задачей.
