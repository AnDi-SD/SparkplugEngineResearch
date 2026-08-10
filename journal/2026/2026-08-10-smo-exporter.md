# 2026-08-10 — первый GLB/OBJ-экспорт SMO

## Вопрос

Можно ли получить из подтверждённой PC-геометрии SMO обычный файл, который Blender открывает без специальных дополнений, не затрагивая skeleton и исходный SMO?

## Решение

Добавлены `SmoExporter.Core`, CLI и format checks. Основной формат — самодостаточный GLB 2.0 со встроенными PNG-текстурами. Обязательный compatibility path создаёт OBJ, MTL и соседние PNG.

Каждый `spMeshData` остаётся отдельным mesh. Triangle strips преобразуются в triangle lists. Экспортируются подтверждённые positions, normals, UV0, UV1 и vertex colors. World transforms применяются к вершинам, ось Z отражается для согласования с текущим viewer path, а winding корректируется.

GLB `extras` сохраняет source SHA-256, platform flags, object index/ID, marker, primitive type, vertex format и serialized/runtime stride. Skeleton и weights в GLB пока намеренно отсутствуют.

## Проверка

- Полная solution: 0 warnings, 0 errors.
- `fish.smo`: 1 mesh, GLB structural checks пройдены.
- `bloom_ball.smo`: 6 mesh, GLB и OBJ импортированы Blender 4.5.3; обе текстуры найдены.
- `menu.smo`: 41 mesh, включая layout `0x0100`, structural checks пройдены.
- `bloom_jeans.smo`: all-zero diffuse оказался служебным placeholder. Экспорт такого `COLOR_0` заставлял Blender умножать встроенную текстуру на чёрный цвет; exporter теперь пропускает нулевой diffuse, сохраняя реальные ненулевые vertex colors.

## Следующий эксперимент

Зафиксировать importer contract и выполнить byte-identical round-trip одного mesh payload до любых изменений размеров. Затем заменить геометрию существующего skin slot с rigid weights `(1,0,0,0)` на одну существующую palette bone.
