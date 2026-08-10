# SmoExporter

Текущая версия: **0.1.0**.

Экспортирует подтверждённую PC-геометрию SMO без изменения исходного файла.

- `GLB` — основной самодостаточный формат для Blender: geometry, materials и PNG-текстуры находятся в одном файле.
- `OBJ + MTL + PNG` — обязательный compatibility export.

Каждый исходный `spMeshData` остаётся отдельным mesh. Triangle strips преобразуются
в triangle lists; сохраняются normals, UV0, UV1 и vertex colors, когда decoder
подтверждает соответствующий layout. Skeleton и skin намеренно не экспортируются
на первом этапе.

```powershell
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- model.smo
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- model.smo --output exported --glb
```

Минимальный графический интерфейс:

```powershell
dotnet run --project tools/SmoExporter/SmoExporter.Gui
```

GUI содержит выбор SMO, отдельный выбор папки сохранения, выбор формата (`GLB`,
`OBJ` или оба), экспорт и сброс. По умолчанию предлагается соседняя с исходным
файлом папка `<имя>_export`.

GLB содержит в `extras` SHA-256 исходного SMO, object index/ID, mesh marker,
vertex format и serialized/runtime stride. Эти данные предназначены для будущего
контролируемого импорта в существующие object slots.
