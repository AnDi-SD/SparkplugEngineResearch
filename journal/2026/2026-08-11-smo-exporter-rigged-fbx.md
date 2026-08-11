# 2026-08-11 — rigged GLB/FBX и SAN в SmoExporter

## Вопрос

Расширить отдельное ядро экспортера до практичного FBX: скелет, palettes/skin weights и выбираемые анимации, сохранив SmoViewer и SmoExporter раздельными программами.

## Метод

В общее представление export scene переносятся логическая иерархия `spNode`, локальная bind pose, `spSkin` palettes, inverse bind matrices и четыре joint/weight компонента вершины. Выбранные SAN декодируются существующим `SmoAnimationDecoder` и сопоставляются с узлами по имени.

Полный GLB 2.0 служит промежуточным и самостоятельным форматом. Бинарный FBX создаётся поддерживаемым экспортером Blender 4.x в фоновом режиме; это позволяет не выдавать ограниченный самописный ASCII FBX за полную поддержку формата.

## Проверка

`Dragon.smo` вместе с `dnaf.san` экспортирован в GLB и FBX без предупреждений. Обратный импорт FBX в Blender 4.5.3 обнаружил 15 mesh objects, один armature, 45 bones, actions и 657 F-curves. Разница между 13 исходными mesh sections и Blender objects требует отдельной визуальной регрессии.

## Вывод

Экспортер переносит подтверждённые mesh/material/texture, skeleton/skin и SAN TRS-данные. Камеры, lights, morph targets и неизвестные SAN events пока не экспортируются: их исходная семантика не подтверждена.

## Следующий эксперимент

Сравнить bind pose и несколько кадров Bloom и Dragon в Viewer, GLB и FBX, затем исследовать имена actions и при необходимости свести Blender bake к одному action на выбранный SAN.
