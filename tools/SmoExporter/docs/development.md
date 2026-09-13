# Устройство SmoExporter

GUI / CLI → SmoExporter.Core → общая сцена → writer выбранного формата.

| Каталог | Ответственность |
| --- | --- |
| [SmoExporter.Core](../SmoExporter.Core/) | Выбор сцены, координаты и экспорт |
| [SmoExporter.Gui](../SmoExporter.Gui/) | WPF-интерфейс |
| [SmoExporter.Cli](../SmoExporter.Cli/) | Командная строка |
| [SmoExporter.FormatTests](../SmoExporter.FormatTests/) | Проверки записанного результата |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
