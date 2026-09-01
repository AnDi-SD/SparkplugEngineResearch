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

Абсолютная placement-матрица в том же X/Y/Z Euler convention, что использует
инспектор редактора (`Scale * Rotation * Translation`):

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

Матрица `InvTransform` всегда создаётся по convention Sparkplug: транспонированный
3x3 basis и `-T*A^T`. При открытии старого проекта version 1 математический
inverse масштабированного placement автоматически исправляется, но только если
он однозначно распознан; произвольная несовпадающая пара отклоняется.

`roundtrip` не перезаписывает исходник и завершается ошибкой, если проект без
правок не воспроизводит его точный SHA-256.

`model-info` печатает геометрию, bounds, UInt32-index risk, normals/UV/colors,
skinning, materials и textures до записи. `scene-mesh-info` показывает уже
записанный physical mesh, его texture binding и итоговый material render state.

## Подтверждённые gates

- основной Core-набор: 1 864 assertions;
- real external gate `Alfea02_old.smo` + `shrek.glb`: 25 assertions;
- real collision gate `Alfea02_old.smo`: 22 assertions;
- `data.bin` остаётся неизменным после transform, collision, model и texture
  операций;
- archive/reopen/build, Undo/Redo, RGBA payload и stale-reference checks
  проходят;
- project import worker возвращает forest plans/assets и не создаёт полный
  промежуточный SMO на диске.
- Gate 3/5: полный Core-набор проходит 1 856 assertions; реальные GLB/OBJ/FBX
  project gates проходят 31/46/57 assertions и native matrix 5/5 scene-ready.
- Gate 6: explicit Group 2, exact triangle order, непустой `wxFaceData`,
  registry/inline removal и gameplay collision; native matrix 6/6 scene-ready.

Финальный stress с seed `82744` прошёл 1 000 операций, два промежуточных
archive/build/reopen checkpoint и детерминированную повторную сборку/re-import.
Итоговый SHA-256 —
`2E95388BC62E29A29F7ADE974DF9C09EEE2AE12283E6185648FC59CB943B4884`.
Этот же SMO достиг scene-ready level 28 в нативной игре, прошёл DirectInput
movement/camera probe и завершил контрольное окно без падения.
