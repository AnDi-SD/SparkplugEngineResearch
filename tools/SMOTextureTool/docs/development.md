# Устройство SMOTextureTool

Avalonia GUI → общий Core / Editing → native reader и texture writer.

| Каталог | Ответственность |
| --- | --- |
| [SMOTextureTool](../SMOTextureTool/) | Avalonia-интерфейс, preview и команды |
| [SMOTextureTool.FormatTests](../SMOTextureTool.FormatTests/) | Проверки чтения, замены и сохранения |
| [SMOTextureTool.GuiTests](../SMOTextureTool.GuiTests/) | Проверки пользовательского сценария |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
