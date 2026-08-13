# Changelog

## 0.2.0 — в разработке

- добавлен экспериментальный skinned GLB → SMO writer: импорт `JOINTS_0`,
  `WEIGHTS_0`, inverse bind matrices, vertex colors и material primitives;
- добавлен bind-pose rebase внешней модели на сохранённый skeleton target,
  нормализация weights и автоматическая нарезка по 16-костным palettes;
- GUI распознаёт skinned GLB, сохраняет меню сопоставления костей и позволяет
  отключить rebase для диагностики; неизвестные active joints блокируют запись;
- после изолированного игрового теста skinned GLB writer переведён с RGBA на
  безопасную fixed-size замену только RGB: target Alpha сохраняется, поскольку
  его изменение отдельно подтверждено причиной crash;
- после игрового crash `uzhs` запрещено переписывать вложенные однокостные
  palettes: `bloom_eyes` сохраняет исходную привязку только к `Head`, а
  несовместимый прозрачный primitive переносится в основной body group;
- поскольку сохранение eye-палитры не устранило crash, добавлен диагностический
  набор, независимо проверяющий RGB, Alpha, новую topology и skinned palettes;
- добавлен skinned FBX → GLB → SMO через Blender с общим расширенным автопоиском,
  ручным выбором `blender.exe`, исключением helper meshes без Armature и тем же
  bone/palette/RGB pipeline, что у прямого GLB;
- после игрового crash graph-transplant полностью удалён: он терял неизвестные
  target bindings и делал нерабочими даже ранее совместимые модели;
- восстановлен консервативный SMO → SMO writer: полный target object graph, IDs,
  skeleton, service objects и неизвестные ссылки сохраняются, меняются только
  mesh/texture leaf payload и reference-only skin palettes;
- отменён неподтверждённый atlas fallback Faragonda `3 → 2`: структурно валидный
  файл с увеличенным texture leaf вызывал crash игры; сочетание с большим числом
  donor texture groups теперь блокируется до записи;
- версия выделена для следующего цикла разработки Importer;
- добавлен безопасный режим SMO → SMO: строгая проверка serializer/platform,
  имён костей и иерархии, а также предупреждение о различии bind pose;
- object graph, skeleton nodes, materials, collision, attachments и неизвестные
  объекты сохраняются от target; vertex payload и texture blocks с Alpha берутся
  от SMO-донора;
- triangles донора автоматически переразбиваются по существующим target skin
  slots с лимитом 16 костей, packed bone indices перенумеровываются по именам;
- поддержано разное число donor/target meshes и palettes: лишние target mesh
  slots сохраняются с невидимым degenerate triangle, дополнительные texture slots
  сохраняют target IDs/ссылки, но получают donor pixel payload;
- добавлен bone mapping planner и раскрываемое дерево GUI: совпавшие bones,
  игнорируемые дополнительные donor bones с фактическим weight fallback и target
  bones без donor influences;
- дополнительные donor bones безопасно сворачиваются в ближайшего общего
  weighted предка или shared bone по bind-позиции; неоднозначная пара блокируется;
- weighted hierarchy сравнивается с пропуском невзвешенных helper/control nodes;
  различающиеся пути показываются в четвёртой ветке bone mapping tree;
- реализована field-wise конвертация подтверждённых skinned vertex layouts,
  включая `0x093E → 0x097E`, генерацию отсутствующих normals и объединение weights;
- writer пересчитывает FFPS catalog offsets/sizes, enclosing object sizes и
  inline reference sizes после изменения visual leaf objects;
- результат повторно проверяется strict parser и SHA-256, а целевой SMO и донор
  запрещено перезаписывать;
- GUI автоматически распознаёт SMO-донор и отключает ненужные подгонку, rigid
  bone, ручную замену atlas и повторную нарезку;
- задокументированы два следующих skinning-пути: name/bind-pose mapping для
  подготовленной модели и контролируемая генерация/редактирование весов для
  модели без корректных костей;
- Importer переведён на общее обновлённое `SmoViewer.Core`, включая compact skinning `0x093E`, уточнённые texture/material bindings и material render flags; формат записи пока намеренно не расширен без отдельных round-trip проверок;
- пользовательская сборка будет формироваться единым упаковщиком как framework-dependent single-file с чистым корнем и проверкой .NET 8 Desktop Runtime.

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
