# Полный read-only разбор `spSkin`

Дата проверки: 2026-08-28. Class ID: `0x681F2043`.

## Итог

`spSkin` оказался не одним полем с bone palette, а трёхсекционным наследником:

1. `spRenderableSerializer` — material, fog, `AlphaSortEnable`, `Priority`;
2. `spModelSerializer` — base mesh и optional `ProjectionGroup`;
3. `spSkinSerializer` — аппаратная bone palette и inverse-bind matrices.

Строгий decoder повторно прочитал исходные directory/PCK-ресурсы и принял все
1 758 уникальных объекта без эвристического поиска байтов:

| Корпус | `spSkin` | SMO | Полный размер объекта |
|---|---:|---:|---:|
| PC working | 748 | 117 | 1 579..732 889 |
| PC pristine | 748 | 117 | 1 579..732 889 |
| PS2 pristine | 262 | 103 | 5 416..485 961 |

Все объекты именованы и физически находятся под `spRenderNode`. Размер меняется
из-за inline mesh/material/node subtrees, а не из-за неизвестных подтипов skin.

## Serializer-секции

Пустой field 0 завершает каждую секцию. В таблице секции нумеруются от конца,
как в `field_definitions` исследовательской базы.

| От конца | Field | Semantic key | Payload | Наличие |
|---:|---:|---|---|---|
| 2 | 0 | `renderable.material` | relationship → `spMaterialData` | optional |
| 2 | 1 | `renderable.fog` | relationship → `spFog` | всегда |
| 2 | 2 | `renderable.alpha_sort` | Boolean `UInt32` 0/1 | всегда |
| 2 | 3 | `renderable.priority` | `UInt32` 0/1/2 | всегда |
| 1 | 0 | `model.base_mesh` | relationship → `spMeshData` | всегда |
| 1 | 1 | `model.projection_group` | `UInt32`, наблюдается только 0 | optional |
| 0 | 0 | `skin.palette` | hint, slot count, node/matrix entries | всегда |

В PC и PS2 executable присутствуют `spSkinSerializer`, `esfSkin`,
`spModelSerializer`, `esfModelBase`, `esfModelProjectionGroup`,
`spRenderableSerializer`, `esfRenderableMaterial` и `esfRenderableFog`.
`esfSkin` — имя всего собственного field 0; отдельные имена двух внутренних
слов и matrix entry в строках executable не найдены.

Всего в базе аннотировано 12 213 нетерминальных полей. Material отсутствует у
двух skin в каждой PC-копии и у одного PS2 skin. ProjectionGroup отсутствует у
43 skin в каждой PC-копии и у двух PS2 skin. Остальные пять полей обязательны.

## Три варианта присутствия

| Вариант | Отличие | PC working | PC pristine | PS2 pristine |
|---|---|---:|---:|---:|
| `skin_full` | все семь semantic fields | 703 | 703 | 259 |
| `skin_no_projection` | нет `ProjectionGroup` | 43 | 43 | 2 |
| `skin_no_material` | нет material relationship | 2 | 2 | 1 |

Комбинации без material и projection одновременно не найдено. Это варианты
присутствия default-полей одного runtime-класса, а не самостоятельные классы.

## Object relationships

Все ненулевые ID однозначно разрешаются в ожидаемый class ID. Base mesh всегда
inline и является физическим ребёнком skin.

| Корпус | Связь | Нет | Sized reference | Inline SBOO |
|---|---|---:|---:|---:|
| PC working | material | 2 | 446 | 300 |
| PC pristine | material | 2 | 446 | 300 |
| PS2 pristine | material | 1 | 1 | 260 |
| PC working | fog | 0 | 645 | 103 |
| PC pristine | fog | 0 | 645 | 103 |
| PS2 pristine | fog | 0 | 169 | 93 |
| PC working | base mesh | 0 | 0 | 748 |
| PC pristine | base mesh | 0 | 0 | 748 |
| PS2 pristine | base mesh | 0 | 0 | 262 |

ID-only форма, допустимая некоторыми старыми `spModel`, у `spSkin` не встречается.

## Layout `skin.palette`

Payload собственного field 0 имеет точную форму:

```text
UInt32 blendInfluenceCountHint
UInt32 slotCount
repeat slotCount times:
    UInt32 spNodeObjectId
    UInt32 inlineSerializedSize
    byte[inlineSerializedSize] inlineSpNodeSBOO
    float32[16] inverseBindMatrix
```

`inlineSerializedSize == 0` означает sized reference. Ненулевой размер точно
совпадает с `SerializedSize` target `spNode`, а inline target является физическим
ребёнком `spSkin`.

`slotCount` — аппаратная ёмкость, а не число реально задействованных костей:

| Корпус | Слотов на skin | Inline node entries | References |
|---|---:|---:|---:|
| PC working | 16 | 1 445 | 10 523 |
| PC pristine | 16 | 1 445 | 10 523 |
| PS2 pristine | 64 | 1 584 | 15 184 |

Таким образом, PC хранит 11 968 palette entries в каждой копии, PS2 — 16 768;
общий проверенный набор содержит 40 704 inverse-bind matrices. Все матрицы
finite, affine и invertible.

Для одной кости матрица может повторяться во многих 16-слотовых PC palettes.
По ключу `(resource, spNode)` найдено 15 093 target-набора; 8 550 targets
повторяются, и у всех повторов inverse-bind совпадает с допуском `0.001`.
Это подтверждает правило `SmoSkinBindingResolver`: bind-world можно получать как
математический inverse сериализованной матрицы только после проверки согласия
всех копий.

## Первое слово: не reserved

Старый decoder ошибочно требовал ноль в первом слове palette payload. Полный
корпус показал значения:

| Значение | PC working | PC pristine | PS2 pristine |
|---:|---:|---:|---:|
| 0 | 713 | 713 | 262 |
| 1 | 19 | 19 | 0 |
| 2 | 6 | 6 | 0 |
| 3 | 6 | 6 | 0 |
| 4 | 4 | 4 | 0 |

У 33 из 35 PC-объектов с ненулевым значением base mesh полностью декодируется;
во всех 33 случаях число точно равно максимальному количеству активных blend
weights у одной вершины. Два оставшихся mesh имеют пока только structural decode.
Поэтому поле названо `BlendInfluenceCountHint`: значения 1..4 подтверждены
корреляцией, а 0 означает omitted/default/unspecified. Точную runtime-роль нуля и
поведение writer ещё нужно проверить в игре.

Контрпример, который раскрыл ошибку: `Characters/Knut/lightbeam_projectile.smo`
имеет hint 1 и корректную 16-слотовую palette. В `SFX/Goopmonster.smo` значения
1..4 следуют фактическому максимуму влияний отдельных mesh chunks.

## PC/PS2

Все 117 общих ресурсов PC working/pristine совпадают побайтно по полным наборам
skin. Между PC pristine и PS2 есть 103 общих пути, но лишь в 14 совпадает число
skin; они дают 30 надёжных ordinal-пар.

| Сравнение 30 пар | Совпало |
|---|---:|
| имя skin | 30 |
| `AlphaSortEnable` | 28 |
| `Priority` | 30 |
| effective `ProjectionGroup` | 30 |
| material target | 30 |
| fog target | 30 |
| base-mesh target | 11 |
| первые 16 node IDs palette | 22 |
| первые 16 inverse-bind matrices | 27 |

Различие 16/64 объясняет, почему произвольное побайтовое сопоставление PC/PS2
palette некорректно. Имена и renderable state при этом сохраняются значительно
лучше, чем разбиение mesh и локальных palettes.

## Реализация и воспроизведение

`SmoSkinDecoder` теперь требует ровно три секции, строгий порядок полей,
разрешённые target classes, корректную inline-топологию и полное потребление
palette payload. Viewer показывает все унаследованные поля и summary palette:
blend-influence hint, число слотов, inline/reference counts и первые node targets.

```powershell
dotnet run --project tools\SmoViewer\SmoViewer.Inspect -- `
  research-db analyze-class local-data\results\smo-corpus-v2.sqlite `
  spSkin --json

python research\analyze_smo_skin.py `
  local-data\results\smo-corpus-v2.sqlite
```

Analyzer идемпотентно записывает семь field definitions, три variants, 1 758
assignments, decoded field values и четыре evidence-записи.

## Открытые вопросы

1. Проверить в PC runtime мутацию `BlendInfluenceCountHint`, особенно переход
   между 0 и фактическими 1..4.
2. Восстановить native PS2 vertex-weight layout, чтобы независимо проверить
   нулевой hint и используемые индексы 64-слотовой palette.
3. Не включать writer для node IDs, inline sizes, slot count и inverse-bind до
   безопасной перестройки object directory и проверки в обеих версиях игры.

Следующий runtime-этап проверяет name binding и существующие character graphs,
не перестраивая palettes/relationships:
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
