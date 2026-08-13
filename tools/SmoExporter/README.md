# SmoExporter

Следующая версия: **0.2.1** (в разработке).

Отдельные ядро, CLI и WPF-приложение для экспорта PC-моделей Sparkplug без изменения исходного SMO.

- `GLB` — самодостаточная сцена с meshes, normals, UV0/UV1, vertex colors, материалами, PNG-текстурами, skeleton/skin и выбранными SAN-анимациями.
- `FBX` — бинарный FBX со скелетом, skin weights, bind pose, материалами, встроенными текстурами и выбранными animations. Для преобразования используется Blender 4.x; `blender.exe` ищется в `PATH`, `BLENDER_PATH`, реестре Windows, системных и пользовательских каталогах установки.
- `OBJ + MTL + PNG` — статический compatibility export.

Каждый исходный `spMeshData` остаётся отдельным mesh. Triangle strips преобразуются в triangle lists. Для skin сохраняются palettes каждого `spSkin`, inverse bind matrices и четыре joint/weight компонента вершины, включая compact layout `0x093E`. SAN translation/rotation/scale tracks сопоставляются с узлами по имени. Rigid-меши экспортируются дочерними узлами соответствующих `spRenderNode`, поэтому аксессуары следуют за их SAN-анимацией.

Флаги `spMaterialData` сохраняют alpha-blend материалы в GLB (`alphaMode=BLEND`) и OBJ (`map_d`). Двухслойный материал экспортируется как базовая текстура по UV0 и первая подтверждённая effect-map как emissive texture по UV1; анимированная последовательность пока отмечается предупреждением и экспортируется первым кадром.

```powershell
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- model.smo
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- model.smo --output exported --glb
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- model.smo --output exported --fbx --animation walk.san
```

Графический интерфейс:

```powershell
dotnet run --project tools/SmoExporter/SmoExporter.Gui
```

GUI выбирает SMO, папку и формат (`GLB`, `FBX`, `OBJ` или все). SAN рядом с моделью находятся автоматически; дополнительные клипы можно добавить файлами или целой папкой и включить галочками. В SmoViewer кнопка «Экспорт…» открывает отдельный SmoExporter с текущей моделью.

При запуске из SmoViewer экспортер получает полный уже сформированный Viewer список SAN и не выполняет повторный поиск. Автопоиск рядом с моделью используется только при самостоятельном открытии SMO в SmoExporter.

При запуске GUI проверяет наличие Blender, показывает найденный путь и блокирует `FBX`/`Все форматы`, если конвертер недоступен. Если автопоиск не нашёл нестандартную установку, в поле Blender можно ввести папку или полный путь к `blender.exe`; кнопка «Файл…» открывает выбор исполняемого файла. Ручной путь проверяется и сохраняется в пользовательских настройках между запусками. Кнопка «Автопоиск» сбрасывает ручной выбор. Нижний журнал отображает подготовку сцены, запись каждого формата, запуск Blender, предупреждения и размеры готовых файлов.

GLB содержит в `extras` SHA-256 исходного SMO, object index/ID, mesh marker, vertex format и strides. Камеры, lights, morph targets и неподтверждённые SAN events не синтезируются: соответствующая семантика исходных файлов пока не декодирована.
