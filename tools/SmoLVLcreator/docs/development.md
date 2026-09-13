# Устройство SmoLVLcreator

GUI → редакторский документ → общие чтение, сцена, Importer, Exporter и Editing.

| Каталог | Ответственность |
| --- | --- |
| [SmoLVLcreator.Core](../SmoLVLcreator.Core/) | Объекты и placements, команды и Undo/Redo |
| [SmoLVLcreator.Gui](../SmoLVLcreator.Gui/) | Окно и взаимодействие |
| [SmoLVLcreator.Viewport.Wpf](../SmoLVLcreator.Viewport.Wpf/) | Редакторские overlays и gizmos |
| [SmoLVLcreator.ProjectTool](../SmoLVLcreator.ProjectTool/) | Команды формата проекта |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
