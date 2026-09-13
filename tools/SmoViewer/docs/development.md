# Устройство SmoViewer

GUI → SmoViewer.Scene / Core → SmoViewer.Sparkplug → общий C++-код.

| Каталог | Ответственность |
| --- | --- |
| [SmoViewer](../SmoViewer/) | Окно, камера, выбор, панели |
| [SmoViewer.Core](../SmoViewer.Core/) | Форматный API и адаптеры |
| [SmoViewer.Scene](../SmoViewer.Scene/) | Подготовка сцены и фактических размещений |
| [SmoViewer.Rendering.Wpf](../SmoViewer.Rendering.Wpf/) | Общий OpenGL backend |
| [SmoViewer.Corpus](../SmoViewer.Corpus/) | Локальный индекс SQLite |
| [SmoViewer.FormatTests](../SmoViewer.FormatTests/) | Синтетические и файловые регрессии |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
