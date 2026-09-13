# SmoViewer 0.3.0

Релиз объединяет просмотр SMO, исследование skeleton/palettes, проигрывание SAN и запуск отдельных инструментов импорта/экспорта.

## Главное

- bind-pose skeleton, attachments, collisions, control/IK nodes и служебные markers;
- подсветка влияния выбранной кости и объектов сцены, кадрирование двойным кликом;
- переключаемые панели ресурсов, слоёв и анимаций;
- SAN/ANM: автопоиск, ручное добавление, групповые фильтры, timeline и CPU skinning;
- исправлены иерархия Bloom, rigid attachments и Dragon mesh `[49]`;
- кнопки «Экспорт…» и «Импорт…» открывают приложения из общего комплекта;
- весь сформированный Viewer список SAN передаётся SmoExporter.

## Комплект

Общий архив содержит самодостаточные Windows x64 приложения:

- `SmoViewer.exe` 0.3.0;
- `SmoExporter/SmoExporter.Gui.exe` 0.2.0;
- `SmoImporter/SmoImporter.Gui.exe` 0.1.1.

.NET устанавливать не требуется. Для экспорта FBX отдельно требуется Blender 4.x; GLB и OBJ работают без Blender.

## Проверка

24 модели Bloom: 4617 assertions; Dragon и Bloom дополнительно прошли exporter/importer format tests. Авторство исправлений Butermix сохранено в `ACKNOWLEDGEMENTS.md`.
