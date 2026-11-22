# SmoLVLcreator.ProjectTool

[English](README.en.md)

Консольная утилита для проектов SmoLVLcreator. Она использует общий `SmoProject`
для воспроизводимого импорта, сборки SMO и регрессионных проверок из командной
строки.

## Формат `.smolvlproj` версии 1

Проект версии 1 хранится в ZIP-контейнере.

- `project.json` содержит версию формата, SHA-256 исходного SMO, слова заголовка
  FFPS, полный нормализованный каталог объектов, стабильные идентификаторы,
  физическое дерево владения, метаданные прямых полей и журнал операций.
- `data.bin` содержит точную исходную секцию данных SMO, включая неизвестные
  поля. Эти байты служат неизменяемой основой проекта.
- `assets/<guid>.bin` содержит неизменяемые сериализованные ветви внешних
  моделей, коллизий и новых размещений, а также точные замены данных объектов.

Для сборки полного SMO используются метаданные, `data.bin` и данные операций.
Планировщик применяет изменения к копии потока, добавляет отсутствующие поля
с подтверждённой схемой, пересчитывает размеры вложенных записей, границы
встроенных объектов и каталог, затем записывает новый контейнер FFPS. Сборка
проекта без правок должна побайтно воспроизвести исходный SMO.

Версия 1 поддерживает следующие операции.

- Правка полей с подтверждённой схемой у импортированных и добавленных объектов.
- Перемещение общего встроенного ресурса с сохранением идентификатора.
- Перенаправление ссылок на другой ресурс того же типа.
- Удаление ветки с сохранением общих потомков.
- Создание размещения со ссылками на существующие меши, материалы и текстуры.
- Добавление готовых сериализованных ветвей объектов.
- Точная замена данных объекта блоком того же размера, включая RGB- и RGBA-текстуры.

История отмены и повтора принадлежит открытой сессии и хранит до 256 транзакций.
Освободившиеся данные истории очищаются автоматически и при вызове `CompactHistory`.

## Команды

Запускайте примеры из корня репозитория. Имена команд и синтаксис аргументов общие
для обоих языков. Справка и локализованные сообщения используют язык интерфейса
Windows. Числовые аргументы используют точку как десятичный разделитель.
Команды с `object-index` принимают индекс в каталоге открытого файла.
`remove-placement` принимает идентификатор объекта в десятичном виде или с
префиксом `0x` для шестнадцатеричного значения.

Для полного списка команд запустите ProjectTool без аргументов.

```powershell
dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- model-info donor.glb

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- scene-mesh-info rebuilt.smo Model_

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- import source.smo level.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- build level.smolvlproj rebuilt.smo

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- roundtrip source.smo level.smolvlproj rebuilt.smo

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- placements level.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- translate level.smolvlproj 1369 1.25 -2.5 3.75 moved.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- set-vector level.smolvlproj 7 transform.scale 1.25 0.75 1.5 scaled.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- remove-branch-preserve level.smolvlproj 1369 removed-safe.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- clone-reference level.smolvlproj 1373 125 0 0 Darch_A02_copy copied.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- remove-placement copied.smolvlproj 0x000010AB restored.smolvlproj
```

Примеры абсолютных преобразований. Инспектор и команды используют углы Эйлера
по осям X, Y и Z и порядок `Scale * Rotation * Translation`.

```powershell
dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- set-placement-trs level.smolvlproj 1373 `
  100 20 -50 15 30 45 1.25 0.75 1.5 transformed.smolvlproj

dotnet run --project `
  tools/SmoLVLcreator/SmoLVLcreator.ProjectTool/SmoLVLcreator.ProjectTool.csproj `
  -c Release -- set-entity-trs source.smo vase09-000 `
  -5400 95 -780 20 30 40 1.2 0.8 1.4 transformed-node.smo
```

Матрица `InvTransform` строится по правилу Sparkplug с транспонированным
базисом 3×3 и переносом `-T*A^T`. При открытии старого проекта версии 1 ядро
исправляет обычную математическую обратную матрицу масштабированного размещения,
если однозначно распознаёт эту пару. Произвольная несовпадающая пара отклоняется.

`roundtrip` защищает исходник от перезаписи и проверяет точный SHA-256 результата
для проекта без правок. Расхождение приводит к ошибке.

`model-info` выводит геометрию, границы модели, сведения о необходимости 32-битных
индексов, нормалях, UV-координатах, цветах вершин, скелетной привязке, материалах и
текстурах до записи. `scene-mesh-info` показывает записанный меш, привязанную
текстуру и итоговое состояние отрисовки материала.

## Контрольные проверки

Ниже приведены результаты отдельных исторических прогонов.

- Основной набор Core прошёл 1 864 проверки.
- Прогон внешней модели `shrek.glb` на `Alfea02_old.smo` прошёл 25 проверок.
- Прогон коллизий на `Alfea02_old.smo` прошёл 22 проверки.
- Проверена побайтовая сохранность `data.bin` при изменениях трансформаций,
  коллизий, моделей и текстур.
- Проверены запись и повторное открытие архива, сборка, отмена и повтор,
  данные RGBA и выявление устаревших ссылок.
- Фоновый импорт внешней модели вернул план добавления ветвей и связанные
  данные. Он использовал временную копию исходного контейнера и сформировал
  промежуточный результат в памяти для извлечения добавляемых ветвей.
- На этапах проверки 3 и 5 полный набор Core прошёл 1 856 проверок. Прогоны
  GLB, OBJ и FBX прошли соответственно 31, 46 и 57 проверок. В пяти из пяти
  нативных прогонов игра достигла состояния готовности сцены.
- На этапе 6 проверены явная группа коллизии 2, точный порядок треугольников,
  непустой `wxFaceData`, удаление из реестра и встроенной ветви и коллизия в
  игре. Во всех шести нативных прогонах игра достигла готовности сцены.

Стресс-тест с начальным числом `82744` прошёл 1 000 операций с двумя
промежуточными проверками записи архива, сборки и повторного открытия. Повторная
сборка и повторный импорт воспроизвели результат с SHA-256
`2E95388BC62E29A29F7ADE974DF9C09EEE2AE12283E6185648FC59CB943B4884`.
Игра достигла готовности уровня 28 с этим SMO, приняла ввод для движения и
камеры через DirectInput и завершила проверочный интервал штатно.

[Устройство редактора и языковые ресурсы](../docs/development.md)
