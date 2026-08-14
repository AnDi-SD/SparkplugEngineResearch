# Sparkplug Engine Research

Исследовательский workspace по движку Sparkplug и ресурсам **Winx Club: The Game** для PC и PlayStation 2. Репозиторий объединяет инструменты, проверяемую документацию и дневник реверс-инжиниринга, но не содержит файлов игры.

Проект не связан с разработчиками или правообладателями игры. Все названия используются только для идентификации исследуемого ПО.

## Текущий вывод

SMO — не просто упаковка одной геометрии. Это little-endian `FFPS`-контейнер с каталогом сериализованных объектов Sparkplug. В исследованном корпусе его граф включает модели, меши, материалы, текстуры, узлы сцены, skin/collision-объекты и другие классы. Игра использует эти связи при загрузке ресурса; точная runtime-семантика отдельных классов ещё исследуется.

Уже работают строгий анализ структуры, просмотр геометрии/материалов/анимаций,
экспорт и контролируемый импорт моделей, а также встроенная проверка результата
нативным загрузчиком игры. Отдельный текстурный инструмент сохраняет безопасную
read-only границу для ещё не подтверждённых writer-сценариев. Текущее состояние
формата описано в [документе SMO](docs/formats/smo.md).

## Состав workspace

| Путь | Назначение |
|---|---|
| [`tools/SmoViewer`](tools/SmoViewer) | Строгий парсер, WPF-просмотрщик и встроенная нативная проверка SMO кодом игры |
| [`tools/SMOTextureTool`](tools/SMOTextureTool) | Read-only Avalonia-инструмент для просмотра и экспорта текстур; resize/repack отключён до отдельной нативной проверки |
| [`tools/SmoExporter`](tools/SmoExporter) | Экспорт SMO в самодостаточный GLB для Blender и OBJ/MTL/PNG для совместимости |
| [`tools/SmoImporter`](tools/SmoImporter) | Visual transplant SMO → SMO, multi-material rigid OBJ/GLB/FBX и skinned GLB/FBX через bone mapping |
| [`tools/WinxHairPatcher`](tools/WinxHairPatcher) | Экспериментальный патчер `WinxClub.exe` для управления внешними волосами fashion-моделей Bloom |
| [`docs`](docs/README.md) | Проверяемые сведения о движке, форматах и различиях платформ |
| [`journal`](journal/README.md) | Хронология экспериментов и принятых решений |
| [`research`](research/open-questions.md) | Очередь открытых вопросов и критерии их закрытия |

Текущий release-candidate цикл: SmoViewer `0.4.0`, SmoImporter `0.3.0` и
SMOTextureTool `2.1.0`. Архивы собираются общей схемой в
`artifacts/release/current` и до ручной проверки остаются черновиками GitHub
Releases. `SmoNativeValidator` сохраняется в исходниках и встраивается в Viewer и
Importer, но отдельной пользовательской программой и отдельным релизом не является.

Оба инструмента подключены как Git submodule и сохраняют собственную историю. Этот репозиторий фиксирует проверенную комбинацию их ревизий.

## Быстрый старт

Требуются Windows и .NET SDK 8 или новее. Общее решение включает WPF-приложение, поэтому полная сборка привязана к Windows.

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
2. [Поиск и загрузка ресурсов Winx Club PC](docs/engine/resource-loading.md)
3. [Формат SMO](docs/formats/smo.md)
4. [Формат STX](docs/formats/stx.md)
5. [PC и PS2](docs/platforms/pc-vs-ps2.md)
6. [Подтверждённые class ID](docs/reference/class-ids.md)
7. [План исследования](ROADMAP.md)
8. [Открытые вопросы](research/open-questions.md)

## Данные игры

Не добавляйте в Git `.smo`, исполняемые файлы, полные каталоги игры, дампы или извлечённые ресурсы. Для них предназначена игнорируемая папка `local-data/`; рекомендуемая организация и правила фиксации результатов описаны в [политике корпуса](docs/research/corpus-policy.md).

Условия использования исходного кода смотрите в соответствующих submodule. Материалы игры в этот репозиторий не входят.
