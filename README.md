# Sparkplug Engine Research

Исследовательский workspace по движку Sparkplug и ресурсам **Winx Club: The Game** для PC и PlayStation 2. Репозиторий объединяет инструменты, проверяемую документацию и дневник реверс-инжиниринга, но не содержит файлов игры.

Проект не связан с разработчиками или правообладателями игры. Все названия используются только для идентификации исследуемого ПО.

## Текущее состояние — 11 сентября 2026

[Основы разработки](docs/engine/software-development-rules.md): инструменты
используют общие восстановленные классы, игра остаётся источником истины.
Работу с ОС, файлами и backend на OpenGL реализуем сами; Vulkan остаётся
целевым API.
Уточнение11 сентября разрешает самостоятельно применять технические ускорения;
их происхождение, ограничения и результаты проверки записываются явно.

Ближайшая цель — рабочие ядра семи приложений `tools/` к14–15 сентября.
[Актуальный срез](docs/research/tools-core-migration-status-2026-09-11.md)
разделяет проверенные операции и остаток каждого ядра. Все ядра ещё не закрыты.
Полное восстановление PC/PS2 executables остаётся дальнейшей целью; PC раньше.

SMO/SAN graph, Node/Model/Skin, материалы, текстуры и анимация подключены
к общему C++ core через `SparkplugViewerNative.dll`. Общий GPU backend
использует material passes/stages, weighted shader lighting, alpha ordering,
runtime mip chains, Text, SkyBox и поддерживаемый Fog. Rigid lighting, полная
проекция игровых материалов в export formats и ряд editing/runtime случаев
остаются в очереди. Платформенные адаптеры не выдаются за исходники игры.

[Цикл11 сентября](docs/research/tools-core-cycle-2026-09-11-1900.md) содержит
21 связанный блок: общие writers, Occlusion/Particle Init, Viewer/LVL output,
native SMO transfer и исправления потребителей. Importer сохраняет полный
donor graph с AnimatedTexture references; geometry-only preview больше
не блокирует многопроходный SMO. [Сквозной контроль](docs/research/tool-final-consumer-checks-2026-09-11.md)
охватывает выбранные операции всех семи приложений. Сборки проверочные,
релиз в этом цикле не упаковывался.

[Измеритель](docs/research/tool-readiness-meter.md) сохраняет исторический
scope16/16 от8 сентября отдельно от нынешней миграции. Проценты изученности
EXE также учитываются отдельно: последнее assessment — PC36,032742%,
PS226,923642%. Подключение backend или успешный повтор теста эти числа
не увеличивает. [Отчёт8 сентября](docs/research/tool-cycle-report-2026-09-08-1900.md)
сохраняет прежние результаты и локальные пакеты как исторический этап.

[Манифест](docs/research/research-manifesto.md) закрепляет постоянную
оптимизацию, выбранные короткие проверки, бюджет RAM около1ГиБ и commits.
Текущее изменение публичных репозиториев отдельно ограничено проверкой
разрешений; локальные контрольные точки не являются публикацией или релизом.

SMO — little-endian `FFPS`-контейнер с каталогом сериализованных объектов Sparkplug. Его граф включает модели, меши, материалы, текстуры, узлы сцены, skin/collision-объекты и другие классы. Для каждой операции инструмента используем подтверждённые части этого графа и реальные правила сериализации. Незатронутые неизвестные данные сохраняются без изменений там, где это доказуемо; недостающие контракты исследуются по мере необходимости.

Уже работают строгий анализ структуры, просмотр геометрии/материалов/анимаций,
экспорт и контролируемый импорт моделей, а также встроенная проверка результата
нативным загрузчиком игры. В рабочей сборке TextureTool 2.2 включены
[проверенные замена RGBA и resize/repack](docs/research/tool-texture-writer-2026-09-08.md)
для поддерживаемых встроенных PC BGRA-текстур. Текущее состояние
формата описано в [документе SMO](docs/formats/smo.md).

## Состав workspace

| Путь | Назначение |
|---|---|
| [`tools/SmoViewer`](tools/SmoViewer) | Строгий парсер, WPF-просмотрщик и встроенная нативная проверка SMO кодом игры |
| [`tools/SMOTextureTool`](tools/SMOTextureTool) | Avalonia: просмотр/PNG, замена RGBA и resize поддерживаемых PC-текстур с сохранением проверенной копии |
| [`tools/SmoExporter`](tools/SmoExporter) | Экспорт SMO в GLB, OBJ и нативный FBX через Autodesk FBX SDK |
| [`tools/SmoImporter`](tools/SmoImporter) | Visual transplant SMO → SMO и импорт rigid/skinned OBJ, GLB и нативного FBX |
| [`tools/SmoLVLcreator`](tools/SmoLVLcreator) | Модульный редактор SMO-уровней: сцена, размещения, коллизии, импорт, экспорт и сохранение |
| [`tools/WinxHairPatcher`](tools/WinxHairPatcher) | Патчер `WinxClub.exe` для управления внешними волосами Bloom в игре и меню костюмов |
| [`tools/SanToVmd`](tools/SanToVmd/README.md) | Python-конвертер SAN → VMD: SMO, SAN и PMD в `input`, готовые анимации в `output` |
| [`tools/WinxRemix`](tools/WinxRemix/README.md) | Наш адаптер Winx Club → RTX Remix, тесты и патчи; оригинальные исходники Remix подключаются извне |
| [`Sparkplug`](Sparkplug/README.md) | Evidence-first реконструкция исходного дерева движка по подтверждённым путям и именам |
| [`docs`](docs/README.md) | Проверяемые сведения о движке, форматах и различиях платформ |
| [`journal`](journal/README.md) | Хронология экспериментов и принятых решений |
| [`research`](research/open-questions.md) | Очередь открытых вопросов и критерии их закрытия |

Последние опубликованные версии инструментов: [SmoViewer `0.5.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smoviewer-v0.5.0),
[SmoExporter `0.5.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smoexporter-v0.5.0),
[SmoImporter `0.6.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smoimporter-v0.6.0),
[SmoLVLcreator `0.1.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smolvlcreator-v0.1.0),
[Winx Hair Patcher `0.2.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/v0.2.0) и
[SMOTextureTool `2.1.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smotexturetool-v2.1.0).
Архивы собраны общей схемой в `artifacts/release/current` и опубликованы в GitHub Releases.
Сборка и native loader smoke-tests не заменяют описанные в release notes визуальные
проверки в игре. `SmoNativeValidator` сохраняется в исходниках и
встраивается в Viewer и Importer, но отдельной пользовательской программой и
отдельным релизом не является.

Первый тестовый выпуск [SmoLVLcreator `0.1.0`](tools/SmoLVLcreator/RELEASE_NOTES_0.1.0.md)
закрывает минимальный цикл редактирования уровня и подготовлен для практической
проверки на копиях игровых SMO. Отложенные улучшения ведутся в его отдельной
[дорожной карте](tools/SmoLVLcreator/ROADMAP.md).

SmoViewer и SMOTextureTool входят в этот репозиторий обычными каталогами `tools/`.
Все приложения и общие библиотеки изменяются и собираются в одном checkout.
Последние опубликованные пакеты Viewer и TextureTool перенесены без пересборки;
их теги сохраняют исходники соответствующих релизов.

## Быстрый старт

Требуются Windows и .NET SDK 9.0.300 или новее. Проекты по-прежнему нацелены на
`.NET 8`, но корневой `.slnx` поддерживается только начиная с SDK 9.0.200, а
генераторы Avalonia 12.1 требуют компилятор из feature band 9.0.300 или новее.
Общее решение включает WPF-приложение, поэтому полная сборка привязана к Windows.

```powershell
git clone https://github.com/AnDi-SD/SparkplugEngineResearch.git
cd SparkplugEngineResearch
dotnet build SparkplugEngineResearch.slnx
```

Пользовательские релизы собираются только через единый упаковщик:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./release/Build-Releases.ps1
```

Он создаёт framework-dependent single-file пакеты с чистым корнем, складывает
приложение, документацию и suite-инструменты в подпапки и отклоняет дублирующиеся файлы.
Корневой загрузчик при необходимости предлагает скачать официальный Microsoft .NET 8
Desktop Runtime, проверяет цифровую подпись Microsoft и устанавливает его в тихом режиме.
Подробный контракт структуры описан в [`release/README.md`](release/README.md).

Запуск просмотрщика и текстурного инструмента:

```powershell
dotnet run --project tools/SmoViewer/SmoViewer
dotnet run --project tools/SMOTextureTool/SMOTextureTool
```

Инспекция одного файла или каталога без GUI:

```powershell
dotnet run --project tools/SmoViewer/SmoViewer.Inspect -- path/to/model.smo
dotnet run --project tools/SmoViewer/SmoViewer.Inspect -- scan path/to/corpus --json
```

Проверки формата являются консольными программами, а не проектами `dotnet test`:

```powershell
dotnet run --project tools/SmoViewer/SmoViewer.FormatTests -- path/to/corpus
dotnet run --project tools/SMOTextureTool/SMOTextureTool.FormatTests -- path/to/corpus
```

`SmoViewer.FormatTests` без существующего пути выполнит синтетические проверки и пропустит corpus checks. `SMOTextureTool.FormatTests` рассчитан на известный локальный набор образцов и требует явный путь к нему.

## Как читать документацию

Мы отделяем наблюдение от предположения:

- **Подтверждено** — воспроизводится кодом или несколькими файлами и имеет понятные границы применимости.
- **Рабочая гипотеза** — объясняет наблюдения, но требует независимой проверки.
- **Открытый вопрос** — данных пока недостаточно.

Начать удобнее отсюда:

1. [Обзор Sparkplug](docs/engine/overview.md)
2. [Оригинальная архитектура Sparkplug/Winx](docs/engine/original-architecture.md)
3. [Runtime-конвейер ресурсов в executable](docs/engine/runtime-resource-pipeline.md)
4. [База файлов, ресурсов и native-прогресса](docs/research/game-resource-database.md)
5. [Поиск и загрузка ресурсов Winx Club PC](docs/engine/resource-loading.md)
6. [Разрешение экрана, камеры и GUI](docs/engine/display-resolution-camera-gui.md)
7. [Формат SMO](docs/formats/smo.md)
8. [Формат STX](docs/formats/stx.md)
9. [PC и PS2](docs/platforms/pc-vs-ps2.md)
10. [Подтверждённые class ID](docs/reference/class-ids.md)
11. [План исследования](ROADMAP.md)
12. [Открытые вопросы](research/open-questions.md)

## Данные игры

Не добавляйте в Git `.smo`, исполняемые файлы, полные каталоги игры, дампы или извлечённые ресурсы. Для них предназначена игнорируемая папка `local-data/`; рекомендуемая организация и правила фиксации результатов описаны в [политике корпуса](docs/research/corpus-policy.md).

Условия использования исходного кода инструментов сохранены рядом с ними:
[SmoViewer](tools/SmoViewer/LICENSE.txt) и
[SMOTextureTool](tools/SMOTextureTool/LICENSE.txt).
Материалы игры в этот репозиторий не входят.
