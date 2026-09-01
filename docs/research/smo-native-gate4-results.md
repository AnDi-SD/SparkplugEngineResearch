# Gate 4: skinned import, bone palettes и штатные SAN

Дата проверки: 29 августа 2026 года.

Статус: **пройден**. Production-путь SmoImporter создаёт skinned PC SMO для двух
различающихся target rigs, сохраняет target graph и bind pose, проходит строгий
декодер, реальную загрузку WinxClub.exe до scene-ready и вычисляемую SAN-деформацию.

## Контрольные данные

| Target | Donor | Output | Вершины | Треугольники | Палитры | SHA-256 |
|---|---|---|---:|---:|---:|---|
| `bloom_jeans.smo` | `hatsune_miku_v6 (1).glb` | `bloom-miku.smo` | 72 548 | 248 223 | 6 | `6734C5FE801657CB12C5AE86EAE97261D0B304B075603BF2B7D40CAF23564B2D` |
| `Flora.smo` | `hatsune_miku_v6 (1).glb` | `flora-miku-safe.smo` | 56 614 | 248 223 | 8 | `05F9E33EE1A28BBA463C41E3FE5E13106E86B6871D0BB55C58D365D1579CB97D` |

Повторная полная подготовка и запись дала те же SHA-256 для обоих файлов.
Исходный donor имеет SHA-256
`6015CA12B96921FBFBC63B4DA41D2E239AEA74785B0B5E2FF52CBD2025ABA46A`.

## Найденная native-only ошибка

Первая версия Flora проходила строгий parser, но игра отвергала её:

```text
Cannot create object of this type (class ID: 0xB19CF1A0)
Resource load failed (name Pelvis, classID: 0x695C0F65)
No empty skin->bone (NULL bone) relation allowed.
```

`0xB19CF1A0` оказался не неизвестным классом. Это четыре байта следующей
inverse-bind matrix, которые loader принял за class ID после неразрешённой ссылки.
Изменяемая палитра skin `[63]` получила ссылку на `Pelvis`, хотя этот node впервые
создаётся физически позже, inline внутри skin `[69]`. Каталог SMO позволял
статическому decoder найти объект, но native `spSkinSerializer` разрешает palette
references в один проход и не использует object directory как таблицу forward
references.

Исправление в `SmoSkinnedGlbReplacer`:

- writable palette получает только кости, физически созданные до своего skin;
- fixed palette с inline-owned nodes не переписывается;
- greedy и exact 16-slot packing используют один load-safe набор кандидатов;
- пустая writable palette получает только ранее созданную fallback-кость;
- непосредственная запись forward reference блокируется до создания output;
- regression-проверка отдельно запрещает forward reference в переписанной
  reference-only palette.

Контрольная лёгкая Flora с donor на 14 779 треугольников падала до исправления тем
же способом и прошла native scene-ready после него. Это отделяет reference-order
ошибку от размеров mesh/index buffers.

## Bone, bind и weight invariants

Production writer и regressions подтвердили:

- точные регистрозависимые bone names и неизменный target object graph;
- неизменные ID, type hash, имена, parent и nesting depth target-объектов;
- target hierarchy, bind-world и inverse-bind matrices;
- отсутствие double-transform в bind frame;
- только конечные нормализованные веса, не более четырёх активных influences;
- 16-slot PC palettes и triangle redistribution без потери 248 223 треугольников;
- все положительные blend indices находятся внутри соответствующей палитры;
- сохранение inline-owned palettes и rigid attachments;
- безопасный отказ при устаревшем/неоднозначном выборе target body или
  недоказуемом fitting.

Для диагностики Inspector получил команду:

```text
SmoViewer.Inspect mesh-inventory <file.smo> [--json]
```

Она выводит по каждому `spMeshData` serialized bytes, vertex/index/triangle count,
disk/runtime stride, owning skin и размер палитры.

## SAN binding и деформация

| Target | SAN | Exact | Case-only | Missing | Ambiguous | Деформировано вершин | Max displacement |
|---|---|---:|---:|---:|---:|---:|---:|
| Bloom | `blwalk.san` | 77 | 0 | `Master` | 0 | 72 548 / 72 548 | 34,4242 |
| Flora | `wfgl.san` | 57 | 0 | `Master` | 0 | 56 614 / 56 614 | 216,8052 |

`Master` — штатный service track: он отсутствует и в pristine target, поэтому не
является регрессией output. Новых missing, case-only или ambiguous bindings нет.
Команда `--stock-animation-deformation` вычисляет две позы по exact именам,
иерархии, bind matrices и фактическим vertex weights; отличие проверяется на
итоговых skinned vertices, а не только на наличии SAN keys.

Viewer теперь применяет SAN names через `StringComparer.Ordinal`, как native PC
runtime. Запуск `SmoViewer.exe model.smo animation.san` автоматически добавляет,
выбирает и проигрывает переданный клип.

## Native matrix

Финальный пакет:
`local-data/validation-results/mvp-gate4-final-native-20260829/run-20260829-173455-359`.

| Case | Logical path | startLevel | Результат | Время |
|---|---|---:|---|---:|
| Bloom/Miku | `Characters\Bloom\bloom_jeans.smo` | 2 | Passed, scene-ready, `CP08` | 27,439 s |
| Flora/Miku | `Characters\Flora\Flora.smo` | 10 | Passed, scene-ready | 28,220 s |

Оба запуска выполнены в принадлежащих validator изолированных workspace; pristine
Media, EXE и registry не менялись. Дополнительный gameplay probe сохранил кадры в
`local-data/validation-results/mvp-gate4-animation-gameplay-20260829`.

## Воспроизведение

```powershell
dotnet run --project tools/SmoImporter/SmoImporter.FormatTests -c Release -- `
  --stock-animation-deformation <output.smo> <stock.san>

dotnet run --project tools/SmoViewer/SmoNativeValidator.Cli -c Release -- `
  --exe local-data/pc-pristine/WinxClub.exe `
  --manifest tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate4-skinned.json `
  --output-dir local-data/validation-results/mvp-gate4-native
```
