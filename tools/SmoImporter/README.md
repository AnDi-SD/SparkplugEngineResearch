# SmoImporter

Первый безопасный importer изменяет только vertex records существующего `spMeshData`
и всегда пишет новый SMO. GUI загружает исходный SMO как серый фон, OBJ/GLB как
оранжевую модель замены и позволяет настроить uniform scale, XYZ rotation и translation.

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.Gui
```

GUI умеет объединить все meshes входной OBJ/GLB-сцены и детерминированно
разрезать triangle stream по настраиваемым `MaxVertices`/`MaxTriangles`.
Каждый chunk получает локальную таблицу индексов; вершины на границах chunks
дублируются. Интерфейс сравнивает число chunks с количеством существующих
`spMeshData` slots исходного SMO.

Текущий подтверждённый whole-model режим:

- объединяет meshes OBJ/GLB в один rigid body-slot с новой topology;
- оставляет остальные существующие render slots валидными через degenerate triangle `(0,0,0)`;
- записывает всем вершинам rigid weight `(1,0,0,0)` на выбранный подтверждённый palette slot;
- принимает embedded GLB base-color либо внешний PNG/JPEG;
- безопасно заменяет только RGB существующего ABGR atlas, сохраняя исходный Alpha, headers, offsets и длину SMO;
- никогда не перезаписывает исходный SMO.

Изменение разрешения texture atlas и texture repack намеренно запрещены: старый
алгоритм `SMOTextureTool` создавал структурно читаемые файлы, которые вызывали
вылет оригинальной игры.

Проверка безопасного export/import round-trip:

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -- path/to/model.smo
```
