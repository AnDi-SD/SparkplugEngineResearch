# SmoViewer 0.4.2

SmoViewer 0.4.2 обновляет общий Windows x64 suite до новых Importer и Exporter.
Parser, viewport, анимации и native validator Viewer функционально соответствуют
версии 0.4.1.

## Состав комплекта

- `app/SmoViewer.exe` — SmoViewer 0.4.2;
- `tools/SmoExporter/SmoExporter.Gui.exe` — SmoExporter 0.4.0;
- `tools/SmoImporter/SmoImporter.Gui.exe` — SmoImporter 0.5.0;
- `tools/WinxHairPatcher/WinxHairPatcher.Gui.exe` — Winx Hair Patcher 0.2.0;
- `native/SmoFbxBridge.exe`, `native/libfbxsdk.dll` и лицензия Autodesk FBX SDK
  2020.3.10 — один общий runtime для Importer и Exporter;
- документация каждого приложения в соответствующей папке `docs`.

FBX теперь читается и записывается напрямую без Blender и Python. Корневой
`SmoViewer.exe` остаётся загрузчиком Microsoft .NET 8 Desktop Runtime x64.

Успешный Viewer/native-loader preview подтверждает чтение и создание ресурса, но
не заменяет визуальную проверку импортированной модели в игре.
