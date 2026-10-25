# Устройство SmoImporter

GUI → SmoImporter.Core → подготовка донора → общий Editing/native writer.

| Каталог | Ответственность |
| --- | --- |
| [SmoImporter.Core](../SmoImporter.Core/) | Доноры, материалы, перенос и подготовка геометрии |
| [SmoImporter.Gui](../SmoImporter.Gui/) | Preview и команды пользователя |
| [SmoImporter.FormatTests](../SmoImporter.FormatTests/) | Проверки операций и неизменности входов |

## Границы ядра

В корне Core находится файл проекта. Исходники сгруппированы по операциям; публичный namespace `SmoImporter.Core` и ссылки между проектами сохранены.

| Группа | Ответственность |
| --- | --- |
| [Input](../SmoImporter.Core/Input/) | Донорская модель, чтение FBX/GLB/OBJ/SMO, режим переноса и входные ограничения |
| [Textures](../SmoImporter.Core/Textures/) | Каталог и чтение текстур, преобразование изображений, оценка памяти и границы сериализации |
| [Geometry](../SmoImporter.Core/Geometry/) | Геометрия mesh, разбиение, подгонка transform и замена ресурса |
| [Rigging](../SmoImporter.Core/Rigging/) | Совместимость скелета, целевой rig, fitting и bake подготовленной позы |
| [GeneratedSkinning](../SmoImporter.Core/GeneratedSkinning/) | Создание весов, семантические области и quality guard |
| [SkinTransfer](../SmoImporter.Core/SkinTransfer/) | Перенос skin/UV и построение skinned visual graph |
| [SceneGraph](../SmoImporter.Core/SceneGraph/) | Добавление, замена и удаление ветвей/размещений, общий lifecycle перезаписи контейнера |

Это группы существующего managed-конвейера, а не новые копии игровой логики. Размещение в подпапке не делает крупный класс декомпозированным: сложные prepare/rebuild и GUI-команды требуют отдельного разбора по контрактам. Не создавать папку на каждый helper или смешивать независимые обязанности в универсальном Common.

## Документация и проверка

Контракт скелета находится в [bone-binding](bone-binding.md), документы выпусков — в [releases](releases/README.md). Исходное содержание описаний выпусков сохраняется; manifest упаковки использует новые пути и прежние имена в готовом пакете.

После изменения Core собираются `SmoImporter.FormatTests` и GUI с актуальной native DLL. FormatTests — исполняемый набор отдельных сценариев: команда без аргументов завершается ошибкой, поэтому нужный режим берётся из `Program.cs`. Например:

```powershell
dotnet run --no-build --project tools/SmoImporter/SmoImporter.FormatTests -c Release -- --texture-pixel-comparison-regression
dotnet run --no-build --project tools/SmoImporter/SmoImporter.FormatTests -c Release -- --generated-quality-guard-regression
```

Для native authoring и реального импорта выбрать соответствующий сценарий с отдельным output, затем проверить strict readback. Перенос исходников сам по себе не меняет алгоритмы и не является новой квалификацией деформаций.

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
