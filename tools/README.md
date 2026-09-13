# Инструменты проекта

Приложения хранятся вместе с исходниками и собственными инструкциями. Общие части находятся в этом же репозитории; для сборки нужен полный клон. Возможности рабочего дерева могут отличаться от опубликованного релиза.

| Программа | Назначение | Исходники и инструкция |
| --- | --- | --- |
| SmoViewer | Просмотр объектов SMO, сцены, текстур, скелетов и SAN-анимаций | [Открыть](SmoViewer/README.md) |
| SMOTextureTool | Извлечение PNG и замена поддерживаемых встроенных PC-текстур | [Открыть](SMOTextureTool/README.md) |
| SmoExporter | Экспорт геометрии, материалов, skin и анимаций в GLB/FBX; статический OBJ | [Открыть](SmoExporter/README.md) |
| SmoImporter | Перенос модели и материалов в целевой SMO | [Открыть](SmoImporter/README.md) |
| SmoLVLcreator | Редактор размещений и поддерживаемых свойств SMO-сцены | [Открыть](SmoLVLcreator/README.md) |
| WinxHairPatcher | Настройка внешних волос Bloom в совместимом PC EXE | [Открыть](WinxHairPatcher/README.md) |
| SanToVmd | Перенос SAN-анимаций тела и пальцев на MMD-модель | [Открыть](SanToVmd/README.md) |
| WinxRemix | Экспериментальный адаптер Winx Club к RTX Remix | [Открыть](WinxRemix/README.md) |

## Общие и служебные компоненты

| Компонент | Назначение |
| --- | --- |
| [Общее ядро](shared-core.md) | Зависимости приложений, C++-логика, managed-обвязка и границы backend |
| [SparkplugViewer.Native](SparkplugViewer.Native/README.md) | C ABI для общих восстановленных классов |
| [FbxBridge.Native](FbxBridge.Native/README.md) | Наш мост к внешнему FBX SDK |
| [NativeValidator](SmoViewer/SmoNativeValidator.Cli/README.md) | Явная проверка ресурса с оригинальной PC-игрой |
| [ResearchWorkbench](ResearchWorkbench/README.md) | Повторно используемые средства анализа и запуск локальных экспериментов |
| [RepositoryMaintenance](RepositoryMaintenance/README.md) | Каталог и проверка публичной документации |
| [Release](../release/README.md) | Упаковка программ по отдельной задаче на выпуск |

Форматные спецификации находятся в [базе знаний](../docs/formats/README.md). Общие правила и зависимости сборки — в [CONTRIBUTING.md](../CONTRIBUTING.md).
