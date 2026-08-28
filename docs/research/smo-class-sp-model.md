# Полный read-only разбор `spModel`

Дата проверки: 2026-08-28. Class ID: `0x763277DB`.

## Итог

`spModel` — двухсекционный renderable-контейнер. Первая секция принадлежит
унаследованному `spRenderableSerializer` и связывает объект с материалом, туманом
и параметрами порядка отрисовки. Финальная секция `spModelSerializer` связывает
его с обязательным `spMeshData` и необязательной projection group.

Строгий decoder повторно прочитал исходные directory/PCK-ресурсы и принял все
118 720 уникальных моделей трёх корпусов без эвристического поиска байтов:

| Корпус | Уникальных `spModel` | SMO-ресурсов |
|---|---:|---:|
| PC working | 40 555 | 311 |
| PC pristine | 40 555 | 311 |
| PS2 pristine | 37 610 | 227 |

Все объекты именованы. В исследовательской базе декодированы 705 928
нетерминальных полей, всем 118 720 объектам назначен подтверждённый вариант.
Writer не включён: семантика чтения подтверждена, безопасность мутаций — нет.

## Serializer-секции и поля

Пустой field `0` завершает каждую секцию. Поэтому одинаковый номер поля в разных
секциях не означает одинаковую семантику.

| Секция от конца | Field | Ключ | Payload | Обязательность |
|---:|---:|---|---|---|
| 1 | 0 | `renderable.material` | object relationship → `spMaterialData` | optional |
| 1 | 1 | `renderable.fog` | object relationship → `spFog` | optional |
| 1 | 2 | `renderable.alpha_sort` | `UInt32`, только `0` или `1` | обычно присутствует |
| 1 | 3 | `renderable.priority` | `UInt32` | присутствует вместе с field 2 |
| 0 | 0 | `model.base_mesh` | object relationship → `spMeshData` | обязательно |
| 0 | 1 | `model.projection_group` | `UInt32` | optional |

`esfModelBase`, `esfModelProjectionGroup`, `esfRenderableMaterial` и
`esfRenderableFog` присутствуют и в PC `WinxClub.exe`, и в PS2 ELF рядом с
регистрациями `spModelSerializer` и `spRenderableSerializer`. Названия
`AlphaSortEnable` и `Priority` дополнительно согласуются с таким же
унаследованным renderable-блоком `spSkin`.

У самого `spModel` нет position/rotation/scale. Размещение приходит от
`spNode`/`spRenderNode` либо от world transform содержащего
`spStaticRenderObject`; inline mesh/material bytes не являются transform-полями
модели.

## Семь вариантов присутствия полей

Это варианты одной структуры, а не семь разных runtime-классов.

| Вариант | Renderable fields | Model fields | PC working | PC pristine | PS2 pristine |
|---|---|---|---:|---:|---:|
| `model_full` | 0,1,2,3 | 0,1 | 37 489 | 37 489 | 37 450 |
| `model_no_projection` | 0,1,2,3 | 0 | 2 886 | 2 886 | 31 |
| `model_no_fog` | 0,2,3 | 0,1 | 51 | 51 | 51 |
| `model_no_fog_no_projection` | 0,2,3 | 0 | 3 | 3 | 0 |
| `model_no_material` | 1,2,3 | 0,1 | 81 | 81 | 78 |
| `model_no_material_no_projection` | 1,2,3 | 0 | 43 | 43 | 0 |
| `model_legacy_compact` | 0,1 | 0 | 2 | 2 | 0 |

Два legacy-объекта находятся в
`levels/gardenia/test_world_winx.smo`: `ramp-000` и `world_box-000`. Они
одновременно опускают `AlphaSortEnable`, `Priority`, `ProjectionGroup` и хранят
все три связи в четырёхбайтовой ID-only форме. Это валидный старый layout, а не
повреждение файла.

## Object relationships

Каждая присутствующая связь разрешается ровно в ожидаемый class ID. Нулевых и
неразрешённых ссылок нет; отсутствие material/fog выражается отсутствием поля.

| Корпус | Связь | Отсутствует | ID-only | Sized reference | Inline SBOO |
|---|---|---:|---:|---:|---:|
| PC working | material | 124 | 2 | 5 028 | 35 401 |
| PC working | fog | 54 | 2 | 40 229 | 270 |
| PC working | base mesh | 0 | 2 | 18 654 | 21 899 |
| PC pristine | material | 124 | 2 | 5 028 | 35 401 |
| PC pristine | fog | 54 | 2 | 40 229 | 270 |
| PC pristine | base mesh | 0 | 2 | 18 654 | 21 899 |
| PS2 pristine | material | 78 | 0 | 5 381 | 32 151 |
| PS2 pristine | fog | 51 | 0 | 37 366 | 193 |
| PS2 pristine | base mesh | 0 | 0 | 16 979 | 20 631 |

Это объясняет прежние «одинаковые строки» и зависимость соседних объектов:
модель хранит не свободное имя ресурса, а сериализованную связь по object ID;
для inline-варианта размер и type hash должны согласоваться с записью object
directory. Произвольная строка может не вызвать немедленный crash, но перестаёт
описывать тот же объектный граф.

## Числовые поля

`AlphaSortEnable`:

- PC: `0` — 23 349, `1` — 17 204, поле отсутствует у двух legacy-моделей;
- PS2: `0` — 23 132, `1` — 14 478.

`Priority` использует значения `0..30`, но диапазон разрежен. Самое частое
значение — `1` (32 615 PC и 31 948 PS2); затем идут `0`, `2`, `4`, `3`, `8`,
`7` и `18`. Редкие значения нельзя сводить к Boolean или маленькому enum.

`ProjectionGroup` содержит только `0` и `3`:

| Корпус | Отсутствует | `0` | `3` |
|---|---:|---:|---:|
| каждый PC | 2 934 | 35 047 | 2 574 |
| PS2 | 31 | 34 874 | 2 705 |

Отсутствие поля сохраняется decoder-ом как `null`. Для межплатформенного
сравнения оно нормализовалось к эффективному нулю, но writer не должен молча
материализовать field 1 до runtime-проверки default-поведения.

## PC/PS2-сопоставление

Все 311 same-path ресурсов PC working/pristine имеют полностью одинаковые
последовательности и payload-хеши `spModel`.

У PC pristine и PS2 найдено 227 общих путей. В 215 ресурсах совпадает число
моделей, что даёт 32 067 ordinal-пар:

| Проверка | Совпало | Различалось |
|---|---:|---:|
| имя `spModel` | 31 549 | 518 |
| вариант присутствия полей | 32 036 | 31 |
| `AlphaSortEnable` | 31 946 | 121 |
| `Priority` | 32 007 | 60 |
| effective `ProjectionGroup` | 32 023 | 44 |
| имя target material | 32 067 | 0 |
| имя target fog | 32 067 | 0 |
| имя target base mesh | 31 141 | 926 |

Совпадение material/fog при платформенных различиях mesh подтверждает, что это
отдельные связи renderable-слоя, а не части mesh payload. Различия base-mesh
имён и numeric state фиксируются как платформенные данные, а не ошибки parser-а.

## Физическое окружение

Непосредственные родители PC-моделей: `spStaticRenderObject` — 20 469,
`spRenderNode` — 13 489, `spPartitionRenderable` — 6 541, `spSkyBox` — 54 и два
корневых legacy-объекта. На PS2: 20 913, 9 739, 6 907 и 51 соответственно.

Parent relation задаёт хранение/размещение, а поля самой модели задают визуальные
ресурсы. Поэтому одинаковый `spMeshData` может безопасно использоваться
несколькими reference-only моделями с разными `spStaticRenderObject` transforms.

## Реализация и воспроизведение

`SmoModelDecoder` строго требует две секции, проверяет порядок и размеры полей,
совместное присутствие fields 2/3 и class ID всех targets. Viewer показывает
шесть полей как decoded read-only значения. Старые эвристические резолверы
reference-only mesh/material переведены на этот decoder.

```powershell
python research\analyze_smo_model.py `
  local-data\results\smo-corpus-v2.sqlite

dotnet run --project tools\SmoViewer\SmoViewer.Inspect -- `
  research-db analyze-class local-data\results\smo-corpus-v2.sqlite `
  spModel --json
```

Analyzer сверяет точные corpus totals, оба executable, семь вариантов,
PC working/pristine payload equality и 32 067 PC/PS2-пар; затем идемпотентно
обновляет field definitions, decoded values, variants, assignments и evidence.

## Открытые вопросы

1. Подтвердить дизассемблированием и runtime-тестом точное применение
   `ProjectionGroup=3` и default при отсутствующем field 1.
2. Проверить в игре безопасные fixed-size изменения `AlphaSortEnable`,
   `Priority` и `ProjectionGroup` отдельно на PC и PS2.
3. Проверить замену material/fog/base-mesh ссылок с сохранением class ID,
   object ID, inline size, directory bounds и parent topology.
4. До этих проверок не включать writer для `spModel`, несмотря на полный
   read-only decode.
