# FBX: сохранение границы authored skin weights

FBX export раньше принимал non-unit weights и создавал SDK clusters с
`eNormalize`. При деформации этот режим приводит сумму к1, тогда как original
PC `Fixed.rfx` использует сохранённую взвешенную сумму xyz без деления.
[Исходный shader и GPU-проверка](tool-gpu-skinning-shared-picking-2026-09-10.md)
уже установили это различие; повторный original probe не нужен.

Подтверждение целевого формата — установленный Autodesk FBX SDK2020.3.10,
`include/fbxsdk/scene/geometry/fbxcluster.h`, ELinkMode. Режим `eAdditive`
описывает взвешенное смещение относительно исходной позиции/associate model;
одна смена enum не является проверенной заменой игрового абсолютного xyz sum.
`eTotalOne` также не даёт произвольные веса. Автоматическая конверсия не вводилась.

## Исправление

В native FBX host `BindSkin` до создания clusters проверяются активные
влияния и сумма. Разница с1 более0,0001 приводит к `FBX_SKIN_WEIGHT_SUM`.
Negative/nonfinite weights и непригодный joint активного влияния дают
`FBX_SKIN_INFLUENCE`, вместо прежнего молчаливого отбрасывания.
Это ограничение **целевого экспортёра**, а не требование оригинального SMO.
Weight0 не заставляет интерпретировать неиспользованный joint slot.

Существующий staged-output путь `FbxExporter` сохраняет файл пользователя
при ошибке native bridge. Source weights и поддерживаемый SDK cluster output
не меняются. Export без skeleton не проверяет неиспользуемые skin attributes.
Новая native-ошибка проходит через прежнюю явную диагностику приложения.

## Адресная проверка

Использована реальная сцена Bloom_body и временное изменение одного vertex
в памяти. Общий fixture `SkinWeightExportRegression` обслуживает GLB и FBX;
нового параллельного генератора тестовых сцен нет.

- Новый test DLL со старым FBX bridge воспроизвёл silent half-sum export:
  ожидаемый refusal отсутствовал, destination был перезаписан.
- Новый FBX bridge: **19 checks**, 3,075с. Три unit/near-unit outputs,
  семь отказов с сохранением destination и geometry-only без skeleton прошли.
- Неизменённый прежний SDK importer прочитал все три supported FBX до/после.
  Полный import payload geometry/skeleton/weights побайтно совпал для каждой
  пары; каждый363616B. FBX metadata/timestamps не сравниваются как geometry.
  Animation poses и полное поведение сторонних редакторов этим не проверены.
- Тот же общий fixture повторил GLB **10 checks**; его три поддерживаемых
  outputs сохранили прежние bytes.

Managed сборка чистая. Первая native сборка остановилась до компиляции:
MSBuild FileTracker получил E_ACCESSDENIED в restricted sandbox. Идентичная
однопоточная сборка после разрешённого запуска вне песочницы завершилась
успешно. Первый log сохранён; сеть и публикация не использовались.

Локальные captures: `local-data/results/tools-core-cycle-20260910-0730/fbx-skin-weights/`.
Source/SDK/tool/report fingerprints: [manifest](../../research/tools-core-fbx-skin-weights-2026-09-10.json).
Неподтверждённая конверсия non-unit deformation остаётся отдельной задачей.
