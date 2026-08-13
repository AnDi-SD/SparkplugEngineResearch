# Дневник исследования

Дневник хранит ход экспериментов: исходный вопрос, входные данные, наблюдение, решение и следующий проверяемый шаг. Он дополняет спецификацию и сохраняет отрицательные результаты, которые иначе легко повторить через несколько месяцев.

## Записи

### 2026

- [2026-08-13 — подготовка release candidate набора](2026/2026-08-13-release-candidates.md)
- [2026-08-13 — SmoImporter: visual transplant SMO → SMO](2026/2026-08-13-smo-importer-smo-replacement.md)
- [2026-08-13 — материалы персонажей и синхронизация Exporter](2026/2026-08-13-smoviewer-materials-and-export-sync.md)
- [2026-08-12 — релизы SmoViewer 0.3, SmoExporter 0.2 и SmoImporter 0.1.1](2026/2026-08-12-tool-releases.md)
- [2026-08-11 — rigged GLB/FBX и SAN в SmoExporter](2026/2026-08-11-smo-exporter-rigged-fbx.md)
- [2026-08-11 — SAN/ANM и проигрывание анимаций в SmoViewer](2026/2026-08-11-smoviewer-animation-ui.md)
- [2026-08-11 — skeleton/palette UI и аудит исходников Butermix](2026/2026-08-11-smoviewer-skeleton-ui.md)
- [2026-08-11 — MediaPath и глобальный корень ресурсов](2026/2026-08-11-resource-path.md)
- [2026-08-11 — система внешних волос Bloom и первый патчер](2026/2026-08-11-bloom-hair-patcher.md)
- [2026-08-11 — WinxHairPatcher 0.1.1 и проверка сигнатур](2026/2026-08-11-bloom-hair-patcher-0.1.1.md)
- [2026-08-10 — расширенный PC/PS2-корпус и GUI SMO](2026/2026-08-10-pc-ps2-comparative-research.md)
- [2026-08-10 — первый GLB/OBJ-экспорт SMO](2026/2026-08-10-smo-exporter.md)
- [2026-08-10 — первое ядро безопасного импорта](2026/2026-08-10-smo-importer-core.md)
- [2026-08-10 — релизы SmoViewer 0.2, SmoExporter 0.1 и SmoImporter 0.1](2026/2026-08-10-tool-releases.md)

- [2026-08-01 — от «контейнера с моделью» к объектному графу](2026/2026-08-01-smo-container.md)
- [2026-08-01 — общий FFPS-loader и следы serializer в PC/PS2](2026/2026-08-01-pc-ps2-executable-triage.md)
- [2026-08-08 — текстуры, UV и цвета вершин в SmoViewer](2026/2026-08-08-smoviewer-textures.md)
- [2026-08-08 — первый transform path для составных SMO-уровней](2026/2026-08-08-level-transforms.md)
- [2026-08-08 — режимы камеры SmoViewer](2026/2026-08-08-smoviewer-camera.md)
- [2026-08-08 — навигация, object tree и журнал SmoViewer](2026/2026-08-08-smoviewer-ui.md)
- [2026-08-08 — подготовка релиза SmoViewer 0.1.0](2026/2026-08-08-smoviewer-0.1-release.md)
- [2026-08-08 — текстуры dating-моделей Bloom](2026/2026-08-08-bloom-dating-outfit.md)

## Шаблон новой записи

```markdown
# YYYY-MM-DD — краткое название

## Вопрос
## Корпус и commit инструментов
## Метод
## Наблюдение
## Вывод
## Что не подтвердилось
## Следующий эксперимент
```

Числа corpus scan без ID/класса корпуса и commit parser считаются временными. Когда гипотеза становится подтверждённым знанием, итог переносится в `docs/`, а дневник остаётся историей получения результата.
