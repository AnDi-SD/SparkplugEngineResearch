# Устройство WinxRemix

Launcher → наша PC-прослойка → Remix bridge/renderer с отдельными патчами.

| Каталог | Ответственность |
| --- | --- |
| [direct-camera-bridge](../direct-camera-bridge/) | Передача камеры |
| [skinning-bridge](../skinning-bridge/) | Bridge для skinning |
| [renderer-skinning-fix](../renderer-skinning-fix/) | Патч renderer |
| [server-instance-audit](../server-instance-audit/) | Диагностика экземпляров |
| [third-party](../third-party/) | Описание внешних зависимостей |

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
