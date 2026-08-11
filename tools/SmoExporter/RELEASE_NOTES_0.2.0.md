# SmoExporter 0.2.0

- GLB 2.0 и бинарный FBX со skeleton, skin weights, palettes, bind pose и выбранными SAN-анимациями;
- материалы, встроенные текстуры, normals, UV0/UV1 и vertex colors;
- выбор SAN в GUI, добавление файлов/папки и получение списка из SmoViewer;
- проверка Blender, блокировка FBX при его отсутствии и подробный журнал экспорта;
- атомарная публикация FBX после полного завершения Blender;
- CLI: `--fbx` и повторяемый `--animation`.

Для FBX требуется Blender 4.x. GLB и OBJ внешних конвертеров не требуют. Камеры, lights, morph targets и неизвестные SAN events пока не экспортируются.
