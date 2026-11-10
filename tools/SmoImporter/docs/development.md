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

Для проверки повреждённых входов доступны отдельные команды:

```powershell
dotnet run --no-build --project tools/SmoImporter/SmoImporter.FormatTests -c Debug -- --import-texture-recovery-regression <новая-папка>
dotnet run --no-build --project tools/SmoImporter/SmoImporter.FormatTests -c Debug -- --adversarial-fixtures <корень-репозитория> <новая-папка-корпуса>
dotnet run --no-build --project tools/SmoImporter/SmoImporter.FormatTests -c Debug -- --adversarial-reader-case <папка-корпуса> <Id-из-manifest> [geometry]
dotnet run --no-build --project tools/SmoImporter/SmoImporter.FormatTests -c Debug -- --recovered-appearance-writer <static-target.smo> <humanoid-target.smo> <recovered.obj> <recovered.glb> <новая-папка-экспорта>
dotnet run --no-build --project tools/SmoImporter/SmoImporter.FormatTests -c Debug -- --untextured-wing-attachment <winged-target.smo> <wingless-target.smo> <новая-папка-экспорта>
```

Генератор использует локальные pristine-модели и сохраняет маленькие авторские OBJ/GLB, повреждённые копии SMO/SAN и manifest ожидаемых исходов с hashes. Manifest описывает ожидания, а не результаты запуска. Каждый повреждённый вход проверяется отдельным процессом с внешним таймаутом; SAN проходит общий native reader. Проверка полной сцены и чтения только геометрии выполняется отдельно: второй режим намеренно не требует пригодных исходных skin weights.

Восстановление изображений принадлежит `ImportedTextureRecovery`: оно удаляет только непригодные изображения и их привязки, сохраняет имена для ручной замены и добавляет `TEXTURE_*` предупреждения с материалами и мешами. Ограничения памяти и ошибки структуры bufferView остаются отказом. GLB сохраняет авторский alphaMode; OBJ/FBX по-прежнему определяют прозрачность по изображению. `MainWindow.Notices.cs` показывает замечания и ошибки независимо от скрытых полей старого редактора; `MainWindow.SourceLoading.cs` читает исходный SMO в фоне и применяет результат только для актуального запроса.

Ожидание расчёта весов до настройки размера и скелета — обычное состояние мастера. `_generatedSkinningPreparationIssue` хранит только реальные отказы; информационные сообщения не должны блокировать переход или попадать в красную панель ошибки. Готовность записи проверяется отдельно по актуальности подготовленной сцены и плана.

Общий `WizardProtectionProgressPanel` принадлежит `DonorPreviewHost`: это слой поверх нижней части предпросмотра, соседний с `DonorPreviewSurface`. Начало, обновление, отмена и повтор любого расчёта защиты используют этот единственный блок через `RefreshWizardProtectionProgress`. Он не участвует в раскладке меню и не меняет размеры viewport; события мыши панели не проходят через поверхность управления моделью.

`MainWindow.StepHelp.Content.cs` задаёт инструкции, доступные ссылки и ключ их актуальности; `MainWindow.StepHelp.cs` управляет общей панелью и поиском элементов. Инструкция не показывается на первом шаге и не пересоздаётся при каждом обновлении прогресса. Высота инструкции и общей области сообщений ограничена размером окна; навигация находится вне их прокрутки. Ссылки переключают только разделы представления защиты, прокручивают элемент в область видимости и добавляют прозрачный для мыши `Adorner` на два плавных цикла. Команды модели, фокус и свойства оформления цели не изменяются. Подсветка и отложенный показ отменяются при новом запросе, смене шага, сворачивании инструкции, скрытии или выгрузке цели либо закрытии окна. Если системные анимации отключены, используется краткая статичная рамка.

`SmoWingFitter.Fit` применяет высоту крыльев и угол к исходному снимку геометрии, без накопления преобразований. Поворот зеркален для двух сторон и идёт в каждой плоскости вокруг внутреннего края у спины с учётом высотного смещения. Матрицы нативных узлов анимации не меняются; исправленная геометрия попадает в attachment draft, предпросмотр и сохраняемый SMO. Параметры входят в состояние мастера, историю и проверку актуальности результата.

Принятие и восстановление позы проходят через общий `PublishWizardRigPose`: после изменения revision он пересчитывает prepared/adapted-план. Сброс generated protection очищает только принадлежащий ей план, сохраняя план другого режима. «Результат» использует сериализованную сцену; позднее обновление камеры не возвращает поверх неё skeleton/legend предыдущего шага.

Общие исходники, native ABI и границы managed-слоя описаны в [общем ядре](../../shared-core.md). Алгоритмы форматного чтения и игрового поведения не копируются в интерфейс этого инструмента.

[Сборка и правила](../../../CONTRIBUTING.md) · [Руководство](../README.md).
