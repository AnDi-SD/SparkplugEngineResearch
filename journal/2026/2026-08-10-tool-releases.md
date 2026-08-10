# 2026-08-10 — релизы SmoViewer 0.2, SmoExporter 0.1 и SmoImporter 0.1

## Состав

- `SmoViewer 0.2.0` выпускается из отдельного репозитория `tools/SmoViewer`;
- `SmoExporter 0.1.0` и `SmoImporter 0.1.0` выпускаются из основного репозитория;
- каждый GUI публикуется отдельным framework-dependent архивом Windows x64;
- игровые ресурсы и содержимое `local-data` в архивы не включаются.

## Основание релиза

Exporter прошёл 12 format assertions на чистом `bloom_jeans_old.smo`; GLB и OBJ
проверялись в Blender. Importer прошёл 12 format assertions, а single-slot rigid
mesh и fixed-size RGB texture replacement независимо проверены загрузкой в игре.
Texture resize/repack исключён из релиза как воспроизводимо аварийный путь.

Viewer 0.2 включает накопленные после 0.1 изменения skin/bone placement, новые PC
layouts и texture formats, UV1, texture sequences, vertex-color modulation и GUI
class/color fallback. SAN/ANM deformation остаётся за границами релиза.
