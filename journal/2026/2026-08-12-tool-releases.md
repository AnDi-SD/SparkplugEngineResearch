# 2026-08-12 — релизы SmoViewer 0.3, SmoExporter 0.2 и SmoImporter 0.1.1

## Состав

- основной архив `SmoViewer-0.3.0-suite-win-x64.zip` содержит Viewer и вложенные release-версии Exporter/Importer;
- все три GUI публикуются self-contained для Windows x64, поэтому установка .NET не требуется;
- Viewer находит инструменты в соседних папках `SmoExporter` и `SmoImporter`;
- Blender остаётся внешней зависимостью только для бинарного FBX.

## Проверка

- Viewer: 24/24 моделей Bloom, 4617 assertions;
- Exporter: Bloom и Dragon по 14 assertions;
- Importer: Bloom и Dragon по 12 assertions;
- release binaries проверяются по версиям файлов и запуском из распакованной структуры.

## Ограничения

В архивы не входят игровые ресурсы и `local-data`. FBX без Blender заблокирован интерфейсом. Importer 0.1.1 сохраняет экспериментальные ограничения fixed-size atlas и существующего object graph.
