# 2026-08-24 — выпуск Importer 0.5.0, Exporter 0.4.0 и Viewer 0.4.2

## Состав выпуска

- [SmoImporter 0.5.0](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smoimporter-v0.5.0);
- [SmoExporter 0.4.0](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smoexporter-v0.4.0);
- [SmoViewer 0.4.2 suite](https://github.com/AnDi-SD/SmoViewer/releases/tag/v0.4.2).

Корневой репозиторий публикуется в фактическую default-ветку `main`. Отдельные
репозитории SmoViewer и SMOTextureTool используют `master`. Локальные игровые и
сторонние модели, логи диагностики и промежуточные build-каталоги в Git и релизные
архивы не включены.

## Проверка

- Release-сборки GUI, core и format-test проектов завершены без ошибок;
- SmoViewer: 160 синтетических assertions;
- SMOTextureTool: round-trip 14 эталонных SMO и BGRA HD `1024×1024`;
- SmoExporter: 56 format assertions, четыре проверки поиска native FBX и
  нативный GLB ↔ FBX round-trip;
- SmoImporter: regressions ресурсов, отмены, normals, Alpha, material groups,
  topology normalization, Head/Hand topology, пальцевых цепочек и сагиттальной
  стены ног;
- реальный Miku stress-case: 50 553 вершины, 76 компонентов, 52 активных сустава,
  ручной Head-эллипсоид `3×`, 15 515 защищённых вершин, writer-plan допустим;
- реальный Flora FBX: 30 meshes, 11 121 вершина, нативная загрузка без Blender;
- каждый ZIP содержит ровно один общий комплект `native/` из трёх файлов.

Консервативное правило головы намеренно отвергает незамкнутую синтетическую
оболочку без доказанного neck cut. Отдельный ограниченный fixture подтверждает
положительный случай головы, а отрицательная проверка гарантирует, что такая
геометрия не получит ошибочные жёсткие веса автоматически.

## Артефакты

| Архив | Размер | SHA-256 |
| --- | ---: | --- |
| `SmoImporter-0.5.0-win-x64.zip` | 8 382 790 байт | `758338abdaa3213be0e9b0e430a3a80a307a8eca3d9ae954eddf6158e7e26a28` |
| `SmoExporter-0.4.0-win-x64.zip` | 6 845 940 байт | `dd2eba6d68bf8a19b30ad5073bbe003c27e22ffbc66197a988b703866d86aef1` |
| `SmoViewer-0.4.2-suite-win-x64.zip` | 12 484 233 байта | `84f9f6b801311e5ce8d8c1d14a34de1ccd4816a3647dd258b1b52628ff5d2bc4` |

К каждому GitHub Release приложен общий `SHA256SUMS.txt`. Архивы являются
framework-dependent single-file пакетами для Windows x64; корневой загрузчик
при необходимости предлагает установить Microsoft .NET 8 Desktop Runtime x64.

## Остаточная проверка

Структурные и round-trip тесты не заменяют визуальную проверку материалов,
прозрачности, depth sorting, нормалей и анимаций непосредственно в игре. Для
созданных Miku/Flora SMO остаётся отдельный игровой прогон на нескольких
анимациях рук, ног, головы и прозрачных деталей.
