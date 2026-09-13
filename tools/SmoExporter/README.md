# SmoExporter

Экспорт PC-ресурсов Sparkplug в GLB, FBX и OBJ с сохранением исходного SMO. Рабочее дерево — **0.7.0**, опубликованная линия — **0.5.0**.

## Использование

Откройте SMO, выберите формат, ресурсы и нужные SAN-анимации, затем каталог результата. Профиль **Авто / Персонаж / Уровень** определяет доступные операции. Для уровня можно экспортировать всё, физическую геометрию, выбранные меши или размещения.

```powershell
dotnet run --project tools/SmoExporter/SmoExporter.Gui
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- model.smo --output exported --glb
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- model.smo --fbx --animation walk.san
dotnet run --project tools/SmoExporter/SmoExporter.Cli -- level.smo --glb --preserve-instances
```

Без переключателя формата CLI записывает GLB и OBJ. `--help` показывает параметры. Команды выполняются из корня репозитория.

| Формат | Особенности |
| --- | --- |
| GLB | Геометрия, материалы, skin, поддерживаемая анимация, общие mesh-инстансы |
| FBX | Геометрия, skin и SAN через FBX SDK; нужен совместимый `SmoFbxBridge.exe` |
| OBJ | Статическая геометрия и материалы; без скелета, SAN и ссылочных mesh-инстансов |

Поддержка экспорта не гарантирует воспроизведение любого игрового шейдера. Неизвестные события SAN и другие неподдерживаемые сущности не синтезируются.

[Подробное руководство и ограничения форматов](docs/guide.md) · [Устройство](docs/development.md) · [Сборка](../../CONTRIBUTING.md) · [FBX-мост](../FbxBridge.Native/README.md).
