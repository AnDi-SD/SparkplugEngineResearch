# SmoLVLcreator.ProjectTool

Вспомогательный CLI для основного проектного формата SmoLVLcreator. Он использует
тот же `SmoProject` Core, что и GUI, и предназначен для воспроизводимых импортов,
сборок и регрессионных проверок без запуска WPF.

## `.smolvlproj` version 1

Формат версии 1 зафиксирован как ZIP-контейнер:

- `project.json` хранит версию формата, исходный SHA-256, FFPS header words,
  полный нормализованный каталог объектов, stable IDs, физическое дерево
  владения, метаданные прямых полей и журнал операций;
- `data.bin` хранит точную исходную data section SMO. Она неизменна на всём
  протяжении жизни проекта и включает неизвестные поля;
- `assets/<guid>.bin` хранит immutable object forests внешних моделей,
  коллизий, созданных размещений и точных замен object payload.

В архиве нет `base.smo`. При сборке copy-on-write planner применяет операции к
копии потока, материализует отсутствующие подтверждённые поля, пересчитывает
вложенные размеры, inline-границы и каталог, затем пишет новый FFPS-контейнер.
Нулевой round-trip обязан быть побайтно идентичен исходному SMO.

Поддерживаемые операции версии 1:

- schema-backed правки импортированных и добавленных объектов;
- перемещение shared inline resource без смены object ID;
- перенаправление ссылок на другой ресурс того же типа;
- удаление ветки с сохранением общих потомков;
- reference placement без копирования mesh/material/texture;
- добавление уже сериализованного объектного леса;
- точная same-size замена object payload, включая RGB/RGBA-текстуры.

История Undo/Redo — состояние сессии и в проект не записывается. Она ограничена
256 транзакциями; orphaned history blobs очищаются автоматически и командой
`CompactHistory`.

## Команды

```powershell
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
```

`roundtrip` не перезаписывает исходник и завершается ошибкой, если проект без
правок не воспроизводит его точный SHA-256.

## Подтверждённый gate 0.1.0

- основной Core-набор: 1 676 assertions;
- real external gate `Alfea02_old.smo` + `shrek.glb`: 25 assertions;
- real collision gate `Alfea02_old.smo`: 12 assertions;
- `data.bin` остаётся неизменным после transform, collision, model и texture
  операций;
- archive/reopen/build, Undo/Redo, RGBA payload и stale-reference checks
  проходят;
- project import worker возвращает forest plans/assets и не создаёт полный
  промежуточный SMO на диске.

Финальный stress с seed `82744` прошёл 1 000 операций, два промежуточных
archive/build/reopen checkpoint и детерминированную повторную сборку/re-import.
Итоговый SHA-256 —
`5F0966193AB44DC39B890EDAD2C8EA2CC73E60E586857EAF5725C6CA9853EB6D`.
Этот же SMO принят нативным загрузчиком игры и пережил контрольное окно без
падения.
