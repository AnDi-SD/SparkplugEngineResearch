# Результаты MVP Gate 6: collision

Статус: пройден 29 августа 2026 года для PC writer и SmoLVLcreator.

## Production-контракт

Новая collision branch создаётся как зарегистрированная пара
`spCollisionInfo` + inline `spMeshBV`. Шаблон по-прежнему предоставляет
подтверждённые relationship/layout и world transform, но Group больше не
зависит от случайно ближайшей ветви: writer явно сериализует
`ProductionDefaultCollisionGroup = 2`.

Group 2 выбран как консервативный production-default: это наиболее частое
авторское значение и на PC pristine (2 206 против 1 433), и на PS2 pristine
(1 901 против 1 405). Его семантическое enum-имя по-прежнему неизвестно и в API
не выдумывается. Нативный PC runtime принял итоговую ветвь и полноценный уровень.

Generated `spMeshBV` сохраняет переданный triangle order и не создаёт
неподтверждённые surface metadata: optional `wxFaceData` отсутствует, что даёт
штатные нулевые per-face defaults. При transform существующей коллизии меняются
только вершины/transform; непустой исходный `wxFaceData` остаётся побайтно
идентичным.

## Автоматические проверки

Real-project gate на `Alfea02_old.smo` проходит 22 assertions:

- add, transform и delete существующей и добавленной collision branch;
- link/unlink и Undo/Redo как editor metadata `.smolvlproj`;
- archive/reopen/build и совпадение project writer с immediate writer;
- ровно одна регистрация в правильном `spPartitionSystem`;
- полное удаление `spCollisionInfo`, `spMeshBV` и registry reference;
- точный порядок triangle indices до и после transform;
- побайтовое равенство реально непустого `wxFaceData` до и после transform;
- явный Group 2 и отсутствие выдуманного `wxFaceData` у новой ветви;
- удаление новой ветви возвращает исходный SMO byte-for-byte.

Полный `SmoLVLcreator.CoreTests` проходит 1 864 assertions. В него дополнительно
входит генерация hull непосредственно по world-геометрии visual entity,
regenerate/delete, автоматическая и ручная link/unlink, совместный transform,
reopen и проверка, что итоговый hull после build всё ещё охватывает перемещённый
visual.

## Строгий аудит артефактов

Каталог:
`local-data/validation-results/mvp-gate6-collision-20260829/project-collision-final`.

| Артефакт | Объекты | Collision | Треугольники collision | SHA-256 |
|---|---:|---:|---:|---|
| source / deleted | 4 266 | 132 | 3 333 | `1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF` |
| `collision.smo` | 4 268 | 133 | 3 339 | `13D48AFBDB1FFC3D305B036FD43F2946EF591D5D3FAEDCAF72E6ED0A5855280B` |
| `imported-collision-moved.smo` | 4 266 | 132 | 3 333 | `7BC887F0889468E9F3B630EE8BA771352F28A6C56C577967FB33164DDDE689C6` |
| `collision-moved.smo` | 4 268 | 133 | 3 339 | `49E8DB178222653E0ABFD4D48E42793E6389290CF944AC44E561F1E7B77C241F` |

Все четыре результата проходят строгий Inspector: signature mismatch 0,
render meshes 702/702 decoded, collision meshes полностью декодируются.
`collision-deleted.smo` побайтно совпадает с source.

## Native scene-ready

Tracked manifest:
[`mvp-gate6-collision.json`](../../tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate6-collision.json).

Финальный run:
`local-data/validation-results/mvp-gate6-collision-native-final-20260829/run-20260829-163451-845`.

| Кейс | Результат | Время |
|---|---|---:|
| baseline before | Passed | 21,576 с |
| generated collision, Group 2 | Passed | 19,329 с |
| moved imported collision | Passed | 19,062 с |
| moved generated collision | Passed | 19,879 с |
| generated collision deleted | Passed | 18,965 с |
| baseline after | Passed | 19,205 с |

Итого 6/6: FFPS `0x26`, ненулевой native resource, активный level state 28,
pending state 0, `SCENE01`, crash `none/none`.

## Gameplay и camera collision

Tracked manifest и воспроизводимый helper:

- [`mvp-gate6-collision-gameplay.json`](../../tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate6-collision-gameplay.json);
- [`Run-CollisionGameplayProbe.ps1`](../../tools/SmoViewer/SmoNativeValidator.Cli/scripts/Run-CollisionGameplayProbe.ps1).

Финальный run на точном SHA `13D48A...280B`:
`local-data/validation-results/mvp-gate6-collision-gameplay-final-20260829/gameplay-probe-wall-hold`.

После `SCENE01` helper посылает DirectInput scan code `DIK_W=0x11`. Первый
кадр фиксирует начальную витринную зону, через 12 секунд камера находится у
торцевого ребра коридора, ещё через 8 секунд упирается в сплошную стену. Затем
relative mouse input не выводит камеру за поверхность. Длительный native run
завершился `Passed` за 62,725 с без crash.

Эта проверка подтверждает управление, остановку уровневой collision и camera
collision после добавления новой ветви. Она не является per-object physics
trace и не приписывает конкретной добавленной грани отдельный runtime callback.

## Граница результата

- названия/битовая семантика Group 1 и 2 всё ещё неизвестны;
- generated faces получают штатный unspecified default; UI не редактирует
  `surfaceType`, `flags` или `surfaceID`;
- navigation, portals и route cost не изменяются;
- PS2 подтверждает общий serializer-layout и допустимость Group 2 по корпусу,
  но PS2 writer/emulator runtime не входит в PC MVP.
