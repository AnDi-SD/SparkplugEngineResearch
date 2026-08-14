# 2026-08-14 — release candidates SmoViewer 0.4, SmoImporter 0.3 и SMOTextureTool 2.1

## Задача

Зафиксировать накопленный цикл исследования SMO/STX, подготовить три Windows x64
пакета по единой схеме и передать их на ручную проверку без публичной публикации.

## Версии и границы

- SmoViewer `0.4.0` — Viewer suite с SmoExporter `0.2.1` и SmoImporter `0.3.0`;
- SmoImporter `0.3.0` — отдельный пакет и компонент suite;
- SMOTextureTool `2.1.0` — отдельный пакет;
- `SmoNativeValidator.Core`, CLI и тестовый harness остаются в исходниках.
  Viewer и Importer используют Core как встроенный компонент, но отдельный EXE,
  архив, tag или GitHub Release для Validator не создаётся.

## Содержательные изменения

### SmoViewer 0.4.0

- добавлена нативная быстрая и контекстная проверка SMO через внутренние вызовы
  Winx Club без hash-gate конкретной версии EXE;
- каждый тест запускает игру в изолированной временной рабочей папке с
  `fullScreen=false`, а GUI показывает краткий результат и оставляет подробности
  в JSONL;
- уточнены BGRA layout, material-to-texture связи, alpha/additive passes,
  compact skinning и визуальные ветви после переноса моделей;
- добавлена благодарность `kotwys` и ссылка на его STX GIMP plugin.

### SmoImporter 0.3.0

- SMO → SMO перенесён на catalog-safe visual forest injection с сохранением
  служебного target graph;
- поддержаны несколько visual roots, rigid-ветви, несколько материалов,
  самостоятельные BGRA textures и texture sequences;
- rigid OBJ/GLB/FBX может связывать `matN` и кадры из выбранной папки PNG;
- skinned GLB/FBX получает ровно один bind-pose retarget, а в SMO остаются target
  skeleton, IDs, inverse-bind matrices и animation graph;
- exact-equal palette-padding повторы одного glTF joint канонизируются с remap
  `JOINTS_0`; разные nodes или разные inverse-bind matrices остаются ошибкой;
- после сохранения GUI автоматически выполняет короткий встроенный native test
  и показывает только итог совместимости.

### SMOTextureTool 2.1.0

- для `0x32E3`/`0x43E3` исправлен обязательный marker `00` на `+0x3C`, начало
  BGRA payload на `+0x3D` и поле `height << 8` на `+0x38`;
- добавлены прямоугольные `128×64`, first/last pixel и byte-round-trip regressions;
- GUI writer/repack остаётся отключённым: нативно подтверждён fixed-size путь,
  но произвольный resize/repack пока не объявляется безопасным.

## Структура пакетов

Все архивы создаются только `release/Build-Releases.ps1`. В корне находится
стабильный bootstrap EXE и `release.json`, приложение лежит в `app`, документация
в `docs`, а компоненты Viewer suite — в `tools`. Пакеты framework-dependent и
при необходимости предлагают установить подписанный Microsoft .NET 8 Desktop
Runtime; сам установщик в архивы не входит.

## Подготовленные архивы

| Архив | Размер, байт | SHA-256 |
| --- | ---: | --- |
| `SmoViewer-0.4.0-suite-win-x64.zip` | 1 959 301 | `945516FC419491F8D1D15973D2331AADC55573F786D38F004FA2F98ACCF77784` |
| `SmoImporter-0.3.0-win-x64.zip` | 1 326 305 | `61BB41F3C1EDCC5989872873674E3C72A61C4DF595DAD4209436F8EE63A67FC7` |
| `SMOTextureTool-2.1.0-win-x64.zip` | 12 780 898 | `23C7A274872FAE8EBCBA840A370C611FF773E3D9C25424D7B563DBA80A923877` |

Общий файл `artifacts/release/current/SHA256SUMS.txt` содержит только эти три
кандидата. Старые архивы предыдущего цикла перед сборкой удалены из `current`,
но остаются доступны в опубликованных GitHub Releases и других локальных
release-каталогах.

## Автоматическая и нативная проверка

Финальная проверка release-кандидатов:

- `dotnet build SparkplugEngineResearch.slnx -c Release --no-restore`: успешно,
  0 ошибок; два `NU1900` из-за недоступного nuget.org vulnerability audit;
- `SmoViewer.FormatTests` на `pc-pristine/Media`: 416/416 SMO и 183 669
  assertions;
- `SmoNativeValidator.Tests`: 210 assertions;
- `SMOTextureTool.FormatTests`: 14 эталонных файлов, 170 textures, 14
  byte-identical round-trip variants и прямоугольный `128×64` regression;
- legacy Bloom и Dragon Exporter → GLB → Importer round-trip: по 15 assertions;
- fixed-size texture writer: 17 assertions;
- Faragonda → Bloom: 7 meshes, 3 textures, 1 440 triangles, strict PASS;
- Лейла OBJ/FBX → Bloom: по 7 material groups, 11 textures, 2 sequences и
  2 908 triangles, strict PASS;
- подготовленная Текна → Tecna: 17/17 active joints, 382 triangles, 7 palettes,
  SHA `856BEC79828FE15FC37017E0CE5E436379551AC2DD169A4FBFCA638EBDA818F4`;
- общий упаковщик создал каталоги из 17/6/8 файлов для Viewer/Importer/TextureTool;
  для каждого ZIP побайтно сверены все entries со staging-каталогом, версии
  manifest/bootstrap/application и отсутствие loose DLL/PDB/Validator files.

Во время разработки отдельно нативно подтверждены:

- Faragonda → Bloom, StellaX ↔ Bloom, multi-texture Лейла → Bloom и skinned
  Текна → Tecna строгим parser и нативным загрузчиком;
- для Лейлы быстрый и контекстный тесты с texture sequences;
- для Текны быстрый тест и две контекстные загрузки без serializer errors/crash;
- все нативные запуски текущего цикла выполнялись в оконном режиме.

Зелёный native test подтверждает чтение FFPS, создание ресурса и короткое окно
стабильности, но не заменяет визуальную проверку материалов, позы и анимации.

## Статус публикации

Исходники предназначены для штатных веток `main`/`master`. GitHub Releases будут
созданы как **draft** с ZIP и общим `SHA256SUMS.txt`. До ручного подтверждения
пользователя черновики не публикуются и release tags не считаются выпущенными.
