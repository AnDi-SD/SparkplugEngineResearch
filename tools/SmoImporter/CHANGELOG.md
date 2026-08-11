# Changelog

## 0.1.1 — 2026-08-12

- GUI принимает путь исходного SMO в командной строке для запуска из SmoViewer;
- в SmoViewer добавлена отдельная кнопка «Импорт…» рядом с запуском экспортера.

## 0.1.0 — 2026-08-10

Первый экспериментальный релиз SmoImporter.

- импорт целой OBJ/GLB-сцены и новая triangle-list topology;
- single-slot rigid mesh replacement с ручным выбором подтверждённой bone palette;
- автоматическая подгонка размера, центра, вращения и смещения;
- сохранение остальных render slots через проверенные degenerate triangles;
- embedded GLB base-color и внешний PNG/JPEG;
- подтверждённая игрой fixed-size замена только RGB исходного ABGR atlas;
- исходные Alpha, headers, offsets, object graph и размер texture block сохраняются;
- запись всегда выполняется в новый SMO с повторной проверкой strict parser.

Изменение разрешения atlas, texture repack, скелетная деформация и импорт анимаций не поддерживаются.
