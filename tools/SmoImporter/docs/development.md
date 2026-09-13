# Устройство SmoImporter

GUI → SmoImporter.Core → подготовка донора → общий Editing/native writer.

| Каталог | Ответственность |
| --- | --- |
| [SmoImporter.Core](../SmoImporter.Core/) | Доноры, материалы, перенос и подготовка геометрии |
| [SmoImporter.Gui](../SmoImporter.Gui/) | Preview и команды пользователя |
| [SmoImporter.FormatTests](../SmoImporter.FormatTests/) | Проверки операций и неизменности входов |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
