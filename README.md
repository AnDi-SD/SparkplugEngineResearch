# Sparkplug Engine Research

Исследовательский workspace по движку Sparkplug и ресурсам **Winx Club: The Game** для PC и PlayStation 2. Репозиторий объединяет инструменты, проверяемую документацию и дневник реверс-инжиниринга, но не содержит файлов игры.

Проект не связан с разработчиками или правообладателями игры. Все названия используются только для идентификации исследуемого ПО.

## Текущий вывод

Текущая цель — [корректная работа наших инструментов](docs/research/tool-driven-research-scope.md).
Исследуем необходимые поля и методы реальных классов, внедряем их в общий код
и проверяем конкретные команды. Полное восстановление движка остаётся
долгосрочной задачей; доводить каждый класс до 100% перед выпуском не требуется.

[Новый измеритель](docs/research/tool-readiness-meter.md) считает подтверждённые
операции инструментов: начальный срез — 11/16 (68,75%). Контракт, внедрение и
проверка результата показываются отдельно; это переучёт существующих функций.
Текущий checkpoint: **16/16 (100% фиксированного списка)**. После
[TextureTool replacement/resize и PNG-проверки](docs/research/tool-texture-writer-2026-09-08.md)
добавлена [совместная FBX alpha](docs/research/tool-fbx-alpha-2026-09-08.md),
исправлен импорт skeleton в Blender и добавлены
[редкие SAN-ключи конвертера](docs/research/tool-san-keys-2026-09-08.md).
В CP128 [общий SAN sampler Viewer 0.6 / Exporter 0.7](docs/research/tool-shared-san-2026-09-08.md)
поддерживает те же rare keys, исправляет привязки и экспорт границ анимации;
[PS2 interval leaf](docs/research/tool-ps2-san-2026-09-08.md) проверен отдельно.
В CP129 [Viewer 0.6.1](docs/research/tool-viewer-performance-2026-09-08.md)
сократил CPU-время анимационного обработчика на двух моделях в 3,1–3,5 раза;
это отдельный замер, а не общий FPS.
В CP130 [исправлены новые текстуры и порядок ссылок Importer](docs/research/tool-importer-textures-2026-09-08.md).
Выявленная регрессия skinned-пути устранена в
[CP131: выбор только необходимых native-шаблонов](docs/research/tool-importer-clean-targets-2026-09-08.md).
Пять целей прошли целый PC loader, точную сверку palettes и штатные SAN в Viewer;
`import.skin` возвращён в готовые. Предыдущий partial snapshot сохранён.
[CP132: запись Importer](docs/research/tool-importer-performance-2026-09-08.md)
на двух целях ускорена на 7–9%, суммарные выделения памяти снижены на 25–27%;
выходные файлы совпадают с прежними по SHA, scope остаётся тем же.
[CP133: подготовка весов](docs/research/tool-importer-preparation-2026-09-08.md)
на двух настоящих донорах ускорена примерно вдвое с точным сохранением результата;
устранён предварительный проход, оставшийся от прежнего palette planner.
[CP134: локальные пакеты](docs/research/tool-release-packages-2026-09-08.md)
содержат актуальные инструкции и версии; проверены состав шести ZIP и запуск
шести приложений плюс трёх suite-инструментов. В
[CP135](docs/research/tool-suite-handoff-2026-09-08.md) фактический комплект
передал SMO/SAN из Viewer в Exporter/Importer и записал FBX, принятый Blender.
Публичная отправка не выполнена.
Это готовность перечисленных вариантов операций; ограничения форматов,
непроверенные варианты файлов и дальнейшие исправления инструментов остаются.

Предыдущий завершённый цикл — [PC SMO/SAN до 07:00 МСК, 8 сентября 2026](docs/research/native-cycle-report-2026-09-08-0700.md):
PC workflow-v2 45,62%, целые сцены, текстурные преобразования и генераторы
частиц; стенд ускорен в 1,86–2,46 раза на контрольных операциях. Эта оценка
и семь широких критериев относятся к исследованию движка, а не готовности
приложений. [Манифест исследования](docs/research/research-manifesto.md)
закрепляет постоянную оптимизацию, бюджет RAM около 1 ГиБ и регулярные commits.

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
| [`Sparkplug`](Sparkplug/README.md) | Evidence-first реконструкция исходного дерева движка по подтверждённым путям и именам |
| [`docs`](docs/README.md) | Проверяемые сведения о движке, форматах и различиях платформ |
| [`journal`](journal/README.md) | Хронология экспериментов и принятых решений |
| [`research`](research/open-questions.md) | Очередь открытых вопросов и критерии их закрытия |

Последние опубликованные версии инструментов: [SmoViewer `0.5.0`](https://github.com/AnDi-SD/SmoViewer/releases/tag/v0.5.0),
[SmoExporter `0.5.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smoexporter-v0.5.0),
[SmoImporter `0.6.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smoimporter-v0.6.0),
[SmoLVLcreator `0.1.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/smolvlcreator-v0.1.0),
[Winx Hair Patcher `0.2.0`](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/v0.2.0) и
[SMOTextureTool `2.1.0`](https://github.com/AnDi-SD/SMOTextureTool/releases/tag/v2.1.0).
Архивы собраны общей схемой в `artifacts/release/current` и опубликованы в GitHub Releases.
Сборка и native loader smoke-tests не заменяют описанные в release notes визуальные
проверки в игре. `SmoNativeValidator` сохраняется в исходниках и
встраивается в Viewer и Importer, но отдельной пользовательской программой и
отдельным релизом не является.

Первый тестовый выпуск [SmoLVLcreator `0.1.0`](tools/SmoLVLcreator/RELEASE_NOTES_0.1.0.md)
закрывает минимальный цикл редактирования уровня и подготовлен для практической
проверки на копиях игровых SMO. Отложенные улучшения ведутся в его отдельной
[дорожной карте](tools/SmoLVLcreator/ROADMAP.md).

Оба инструмента подключены как Git submodule и сохраняют собственную историю. Этот репозиторий фиксирует проверенную комбинацию их ревизий.

## Быстрый старт

Требуются Windows и .NET SDK 9.0.300 или новее. Проекты по-прежнему нацелены на
`.NET 8`, но корневой `.slnx` поддерживается только начиная с SDK 9.0.200, а
генераторы Avalonia 12.1 требуют компилятор из feature band 9.0.300 или новее.
Общее решение включает WPF-приложение, поэтому полная сборка привязана к Windows.

```powershell
git clone --recurse-submodules https://github.com/AnDi-SD/SparkplugEngineResearch.git
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

Если репозиторий уже клонирован без submodule:

```powershell
git submodule update --init --recursive
```

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

Условия использования исходного кода смотрите в соответствующих submodule. Материалы игры в этот репозиторий не входят.
