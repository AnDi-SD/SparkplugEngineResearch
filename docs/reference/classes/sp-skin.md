# spSkin / spSkinSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spSkin](../../../Sparkplug/Code/Sparkplug/spSkin.h), [spSkinSerializer](../../../Sparkplug/Code/Sparkplug/spSkinSerializer.h).

### Идентичность

`spSkin` имеет class ID `0x681F2043` и напрямую наследует `spModel`
(`0x763277DB`). `spSkinSerializer` имеет ID `0x120D33C7`, напрямую наследует
`spModelSerializer` (`0xDB55C34A`) и обслуживает именно `spSkin`.

| Факт | `spSkin` | `spSkinSerializer` |
| --- | ---: | ---: |
| initializer | `0x006D3B20` | `0x006D4880` |
| protected factory entry | `0x0046A120` | `0x00490C50` |
| observed extent | `0x70` | `0x14` |

### Runtime-layout `spSkin`

После PC-layout `spModel` размером `0x60` расположены четыре слова:

| Offset | Тип | Назначение |
| ---: | --- | --- |
| `+0x60` | `uint32` | weight/influence count, первое слово `esfSkin` |
| `+0x64` | `uint32` | число костей/palette slots |
| `+0x68` | `spNode**` | массив отношений к костям |
| `+0x6C` | `float (*)[16]` | параллельный массив inverse-bind матриц |

Эти поля совпадают с уже доказанным wire payload `esfSkin`: два `UInt32`, затем
для каждого слота relationship к `spNode` и матрица `float32[16]`. Название
первого слова как точного original member пока неизвестно; здесь используется аналитическое имя `BlendInfluenceCountHint`.

### Выход в renderer

`spSkin::0x0046A240` выполняет inherited pre-render, проходит все кости,
комбинирует transform каждой `spNode` с соответствующей inverse-bind matrix и
заполняет renderer bone palette. Затем функция публикует `boneCount` в поле
renderer `+0xC9BC`, передаёт base mesh через уже подтверждённый renderer slot
`9`, выполняет post-render и очищает временный bone count **только на успешной
ветви** `0x0046A38B`. Error exits через `0x0046A382` этот сброс не выполняют.

Это замыкает PC-цепочку:

```text
SMO esfSkin -> spSkin arrays -> spNode transforms + inverse bind
             -> renderer bone palette -> spModel base mesh submission
```

### Serializer

Три главных entry point восстановлены без обращения к PS2:

- `0x00490D30` сначала индексирует `spModel`, затем все bone relationships;
- `0x00490DA0` записывает base-секцию, всегда открывает field `0` (`esfSkin`),
  пишет два счётчика и для каждого слота relationship плюс ровно 64 байта
  матрицы, после чего завершает field;
- `0x00491170` сначала читает base, принимает только field `0`, выделяет оба
  массива и читает отношения класса `spNode` (`0x695C0F65`) и матрицы. Null
  bone считается ошибкой и ведёт к нативному диагностическому пути.

### Что ещё неизвестно

Track-to-local-node путь теперь подтверждён через `spActor`,
`spTransformTrackEval` и `spNodeController`. Dirty propagation и cached world
PRS/builder закрыты последующим [PC node-проходом](../../engine/scene/node-world.md).
Открыты outer frame caller и связанная renderer integration.

### Итог

`spSkin` оказался не одним полем с bone palette, а трёхсекционным наследником:

1. `spRenderableSerializer` — material, fog, `AlphaSortEnable`, `Priority`;
2. `spModelSerializer` — base mesh и optional `ProjectionGroup`;
3. `spSkinSerializer` — аппаратная bone palette и inverse-bind matrices.

Строгий decoder повторно прочитал исходные directory/PCK-ресурсы и принял все
1 758 уникальных объекта без эвристического поиска байтов:

| `spSkin` | SMO | Полный размер объекта |
| ---: | ---: | ---: |
| 748 | 117 | 1 579..732 889 |
| 748 | 117 | 1 579..732 889 |
| 262 | 103 | 5 416..485 961 |

Все объекты именованы и физически находятся под `spRenderNode`. Размер меняется
из-за inline mesh/material/node subtrees, а не из-за неизвестных подтипов skin.

### Serializer-секции

Пустой field 0 завершает каждую секцию. В таблице секции нумеруются от конца,
как в `field_definitions` исследовательской базы.

| От конца | Field | Semantic key | Payload | Наличие |
| ---: | ---: | --- | --- | --- |
| 2 | 0 | `renderable.material` | relationship → `spMaterialData` | optional |
| 2 | 1 | `renderable.fog` | relationship → `spFog` | всегда |
| 2 | 2 | `renderable.alpha_sort` | Boolean `UInt32` 0/1 | всегда |
| 2 | 3 | `renderable.priority` | `UInt32` 0/1/2 | всегда |
| 1 | 0 | `model.base_mesh` | relationship → `spMeshData` | всегда |
| 1 | 1 | `model.projection_group` | `UInt32`, наблюдается только 0 | optional |
| 0 | 0 | `skin.palette` | hint, slot count, node/matrix entries | всегда |

В PC и PS2 executable присутствуют `spSkinSerializer`, `esfSkin`, `spModelSerializer`, `esfModelBase`, `esfModelProjectionGroup`, `spRenderableSerializer`, `esfRenderableMaterial` и `esfRenderableFog`.

Всего в базе аннотировано 12 213 нетерминальных полей. Material отсутствует у
двух skin в каждой PC-копии и у одного PS2 skin. ProjectionGroup отсутствует у
43 skin в каждой PC-копии и у двух PS2 skin. Остальные пять полей обязательны.

### Три варианта присутствия

| Вариант | Отличие | PS2 pristine |
| --- | --- | ---: |
| `skin_full` | все семь semantic fields | 259 |
| `skin_no_projection` | нет `ProjectionGroup` | 2 |
| `skin_no_material` | нет material relationship | 1 |

Комбинации без material и projection одновременно не найдено. Это варианты
присутствия default-полей одного runtime-класса, а не самостоятельные классы.

### Object relationships

Все ненулевые ID однозначно разрешаются в ожидаемый class ID. Base mesh всегда
inline и является физическим ребёнком skin.

| Связь | Нет | Sized reference | Inline SBOO |
| --- | ---: | ---: | ---: |
| material | 2 | 446 | 300 |
| material | 2 | 446 | 300 |
| material | 1 | 1 | 260 |
| fog | 0 | 645 | 103 |
| fog | 0 | 645 | 103 |
| fog | 0 | 169 | 93 |
| base mesh | 0 | 0 | 748 |
| base mesh | 0 | 0 | 748 |
| base mesh | 0 | 0 | 262 |

ID-only форма, допустимая некоторыми старыми `spModel`, у `spSkin` не встречается.

### Layout `skin.palette`

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

| Слотов на skin | Inline node entries | References |
| ---: | ---: | ---: |
| 16 | 1 445 | 10 523 |
| 16 | 1 445 | 10 523 |
| 64 | 1 584 | 15 184 |

### Первое слово: не reserved

| Значение | PS2 pristine |
| ---: | ---: |
| 0 | 262 |
| 1 | 0 |
| 2 | 0 |
| 3 | 0 |
| 4 | 0 |

У 33 из 35 PC-объектов с ненулевым значением base mesh полностью декодируется;
во всех 33 случаях число точно равно максимальному количеству активных blend
weights у одной вершины. Два оставшихся mesh имеют пока только structural decode.
Поэтому поле названо `BlendInfluenceCountHint`: значения 1..4 подтверждены
корреляцией, а 0 означает omitted/default/unspecified. Точную runtime-роль нуля и
поведение writer ещё нужно проверить в игре.

Контрпример, который раскрыл ошибку: `Characters/Knut/lightbeam_projectile.smo`
имеет hint 1 и корректную 16-слотовую palette. В `SFX/Goopmonster.smo` значения
1..4 следуют фактическому максимуму влияний отдельных mesh chunks.

### PC/PS2

| Сравнение 30 пар | Совпало |
| --- | ---: |
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

`SmoSkinDecoder` теперь требует ровно три секции, строгий порядок полей,
разрешённые target classes, корректную inline-топологию и полное потребление
palette payload. Viewer показывает все унаследованные поля и summary palette:
blend-influence hint, число слотов, inline/reference counts и первые node targets.
