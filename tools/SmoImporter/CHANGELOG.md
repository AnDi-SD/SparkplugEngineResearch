# Changelog

## 0.3.0 — 2026-08-14

- режим SMO → SMO переведён на append-only visual packing: service/skeleton graph,
  IDs и неизвестные связи target сохраняются, старые meshes остаются невидимыми
  structural anchors, а donor `spSkin`/material/texture/UV/mesh branches получают
  новые уникальные IDs;
- упаковщик переносит visual forest из нескольких render roots, отдельные rigid-
  детали, несколько texture groups и нативные texture sequences без atlas fallback;
  контрольные Faragonda → Bloom и StellaX ↔ Bloom прошли strict и native проверки;
- добавлен rigid multi-material импорт GLB, OBJ и FBX. Материалы связываются с
  PNG через metadata либо OBJ `mtllib`/`usemtl`/`map_Kd`; GUI принимает отдельную
  папку с `matN.png` и последовательностями `matN.1.png`, `matN.2.png`;
- non-POT текстуры увеличиваются до следующей степени двойки по каждой оси,
  никогда не уменьшаются и ограничены 2048 пикселями. Исходные POT-пиксели и
  полный BGRA/Alpha сохраняются;
- исправлено зеркальное поле texture header `+0x38`: оно хранит `height << 8`.
  Формула подтверждена на 2 348 pristine textures и устранила нативный crash
  прямоугольных атласов 128×64;
- skinned GLB/FBX transfer разделён на режимы `RetargetToGameBindPose` и
  `PreservePreparedGeometry`. Production-режим выполняет ровно один bind-pose
  rebase, но записывает только target bone IDs, target graph и canonical target
  inverse-bind matrices — donor nodes и animation logic не переносятся;
- skinned texture replacement ограничен сопоставленным body-atlas. Материалы с
  одним source image объединяются, а глаза, эффекты и непарные texture objects
  остаются побайтно исходными;
- подготовленная Текна перенесена в чистый `Characters/Tecna/Tecna.smo`: 17 active
  joints совпали точно, 382 triangles распределены по семи игровым palettes,
  `tecna_e` сохранена. Результат прошёл strict parser, быстрый native smoke test и
  две контекстные загрузки на Cloud01_02 в оконном режиме;
- исправлен off-by-one texture parser/writer: для `0x32E3`/`0x43E3` marker `00`
  находится на `+0x3C`, а BGRA payload начинается с `+0x3D`;
- после сохранения GUI автоматически проверяет новый SMO нативным загрузчиком
  игры в быстром изолированном оконном режиме. Пользователю доступны только путь
  к `WinxClub.exe` и краткий результат «подходит / не подходит / не определено»;
- GUI не разрешает перезаписать target или donor. Skinned-режим также блокирует
  AutoFit и произвольный transform, которые могли бы повторно применить позу;
- FBX использует общий GLB pipeline через Blender, с расширенным автопоиском
  `blender.exe` и исключением helper meshes без Armature.
- GLB reader распознаёт безопасное palette padding старого SmoExporter: повтор
  одного glTF joint node с побайтно одинаковой inverse-bind matrix схлопывается с
  remap `JOINTS_0`; одинаковые имена разных nodes и разные matrices по-прежнему
  блокируются как неоднозначные.

## 0.2.0 — 2026-08-13

- добавлен экспериментальный skinned GLB → SMO writer: импорт `JOINTS_0`,
  `WEIGHTS_0`, inverse bind matrices, vertex colors и material primitives;
- добавлен bind-pose rebase внешней модели на сохранённый skeleton target,
  нормализация weights и автоматическая нарезка по 16-костным palettes;
- GUI распознаёт skinned GLB, показывает дерево сопоставления костей и блокирует
  запись при неизвестных active joints;
- добавлен skinned FBX → GLB → SMO через Blender с общим bone/palette/RGB pipeline;
- добавлен консервативный режим SMO → SMO: target object graph, IDs, skeleton,
  service objects и неизвестные ссылки сохраняются, меняются visual leaf payload
  и reference-only palettes;
- различающиеся donor bones сворачиваются в ближайшие совместимые target bones,
  а weighted hierarchy сравнивается с пропуском helper/control nodes;
- подтверждённые vertex layouts конвертируются field-wise, включая
  `0x093E → 0x097E`, генерацию normals и объединение weights;
- writer пересчитывает каталог, object sizes и offsets, повторно проверяет
  результат strict parser и не разрешает перезаписать target или donor;
- пользовательский пакет переведён на единый framework-dependent single-file
  формат с загрузчиком .NET 8 Desktop Runtime.

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

Изменение разрешения atlas, texture repack, скелетная деформация и импорт анимаций
в этой версии не поддерживались.
