# `spMaterialData` (`0x6160348B`)

Статус: **полный структурный read-only decode всех наблюдаемых PC/PS2-полей**.
Редактирование намеренно не заявлено безопасным: writer и поведение изменённого
материала в игре ещё не проверены.

## Охват

| Корпус | Уникальные SMO | Уникальные объекты | Физические объекты |
|---|---:|---:|---:|
| `pc-working` | 412 | 36 334 | 36 334 |
| `pc-pristine` | 412 | 36 334 | 36 334 |
| `ps2-pristine` | 314 | 32 920 | 56 532 |

Все 105 588 уникальных объектов безымянные. Строгий decoder прочитал их до
финального поля-терминатора и не встретил ни одного прямого поля вне набора
`0, 1, 2, 3, 4, 6, 8, 9, 10, 11, 12, 17`. В исследовательской БД
аннотированы 736 236 непустых прямых полей.

PC working и pristine содержат одинаковые структуры материалов во всех 412
ресурсах. PS2 physical count больше unique count из-за повторов одних и тех же
SMO в PCK; статистика вариантов считается по unique-ресурсам.

## Полный наблюдаемый layout

`spMaterialData` состоит из общей части, одного–трёх повторяемых проходов и
завершающей цветовой части:

```text
[field 1]                    optional vertex alpha = true
 field 0, 44 bytes           11 × UInt32 material render states

 repeat 1..3 material pass:
   field 3, 4 bytes          UInt32 FinalBlendOp
   field 4, 4 bytes          UInt32 layer class ID
   field 8 or 17, 36 bytes   9 × UInt32 texture states
  [field 9, 40 bytes]        UInt32 enabled + 9 × Single UV matrix
  [field 10]                 relationship to spTextureData
  [field 11]                 relationship to spAnimTexController
  [field 12]                 relationship to spUVController

 field 2, 20 bytes           4 × ARGB UInt32 + Single specular power
 field 6                     relationship to spMaterialColorController
 field 0, 0 bytes            object terminator
```

Порядок строгий. Поле `0` имеет две разные роли, которые однозначно различаются
размером: 44-байтовый render-state block в начале и нулевой terminator в конце.

## Семантика полей

| ID | Semantic key | Payload | Наблюдение |
|---:|---|---|---|
| 0 | `material.render_states` | `11 × UInt32` | обязательный блок; отдельные slot-enum ещё не названы окончательно |
| 1 | `material.vertex_alpha` | true byte | optional; false кодируется отсутствием поля |
| 2 | `material.color` | 4 ARGB + `Single` | ambient, diffuse, specular, emissive, specular power |
| 3 | `material.pass` | `UInt32` | `FinalBlendOp`, одновременно начало следующего прохода |
| 4 | `material.layer` | `UInt32` class ID | во всём корпусе только `0x234C576B spStdLayer` |
| 6 | `material.color_controller` | object relationship | обязательное поле: null или inline `spMaterialColorController` |
| 8 | `material.texture_states_legacy` | `9 × UInt32` | старый `spTextureStateBlockOld` |
| 9 | `material.static_uv_transform` | bool + `Matrix3x3` | optional; матрица хранится row-major |
| 10 | `material.texture` | object relationship | optional `spTextureData` |
| 11 | `material.animation_controller` | object relationship | optional `spAnimTexController` |
| 12 | `material.uv_controller` | object relationship | optional `spUVController` |
| 17 | `material.texture_states` | `9 × UInt32` | текущая форма texture-state block |

Field `8` и `17` взаимозаменяемы на уровне структуры и никогда не смешиваются
в одном материале. В 30 сопоставленных PC/PS2-материалах одинаковые девять
значений сохранены под разными номерами поля; поэтому это версия контейнера, а
не самостоятельное различие render state.

Все 1 108 встретившихся field `9` имеют `enabled = 1`. Decoder всё равно
принимает оба корректных Boolean-значения и проверяет конечность всех девяти
`Single`.

## Кодирование object relationship

Поля `6`, `10`, `11` и `12` используют три реально встречающиеся формы:

| Размер | Форма | Значение |
|---:|---|---|
| 4 | null/legacy ID-only | `UInt32 objectId`; ноль означает null |
| 8 | reference | `UInt32 objectId`, `UInt32 inlineSize = 0` |
| `8 + N` | inline | `objectId`, `inlineSize = N`, затем полный `SBOO` объекта |

Strict decoder проверяет ID и ожидаемый class hash по object catalog, а для
inline-формы также `SBOO`, заявленный размер и тип дочернего объекта. Три PC
texture relationship в `staff_projectile.smo` используют редкую legacy
четырёхбайтовую ID-only форму; случайный несуществующий ID проверку не проходит.

Распределение по одному PC-корпусу и PS2:

| Связь | PC | PS2 |
|---|---|---|
| field 10 отсутствует | 7 786 | 6 952 |
| texture inline | 2 513 | 2 301 |
| texture reference | 27 070 | 24 604 |
| texture legacy ID-only | 3 | 0 |
| animation controller inline | 9 | 10 |
| UV controller inline | 1 607 | 1 491 |
| color controller inline | 2 391 | 0 |
| color controller null | 33 943 | 32 920 |

Отсутствие concrete `spMaterialColorController` на PS2 не означает отсутствия
поддержки: serializer и field name присутствуют в PS2 executable, а field `6`
во всех PS2-материалах явно хранит null ID.

## Проходы и структурные варианты

| Вариант | `pc-working` | `pc-pristine` | `ps2-pristine` |
|---|---:|---:|---:|
| current, один проход | 32 347 | 32 347 | 31 952 |
| current, 2–3 прохода | 1 002 | 1 002 | 934 |
| legacy, один проход | 2 974 | 2 974 | 34 |
| legacy, 2–3 прохода | 11 | 11 | 0 |

Отдельно по точному числу проходов:

| Проходов | один PC-корпус | PS2 |
|---:|---:|---:|
| 1 | 35 321 | 31 986 |
| 2 | 988 | 931 |
| 3 | 25 | 3 |

Таким образом, тысячи прежних size/signature-кандидатов не являются тысячами
подтипов материала. Подтверждены четыре storage-варианта, а переменный полный
размер в основном создают повторные pass и inline child objects.

Наблюдаемые значения `FinalBlendOp`:

| Значение | один PC-корпус | PS2 |
|---:|---:|---:|
| 0 | 23 817 | 22 431 |
| 2 | 11 195 | 9 355 |
| 4 | 28 | 16 |
| 5 | 2 | 0 |
| 6 | 2 330 | 2 055 |

Это enum, а не bit mask. Назначать значениям универсальные названия вроде
«прозрачный» пока нельзя: фактический режим зависит также от обоих state blocks,
consumer state, vertex color и alpha текстуры.

## Разнообразие значений

Несмотря на 36 тысяч материалов, собственных state tuples мало:

| Набор | один PC-корпус | PS2 |
|---|---:|---:|
| 11 material render states | 59 | 55 |
| 9 texture states | 18 | 15 |
| material color block | 28 | 27 |

Vertex alpha включён у 621 PC- и 509 PS2-материалов. Static UV transform
присутствует в 379 PC- и 350 PS2-проходах.

Родителями материалов в PC являются преимущественно `spModel` (35 401), затем
`spParticleSystem` (618), `spSkin` (300), `spTextRenderable` (10) и
`spLensFlare` (2); три объекта корневые. На PS2: `spModel` (32 151),
`spParticleSystem` (507), `spSkin` (260) и `spLensFlare` (2).

## Сравнение PC и PS2

По каноническому пути найдено 314 общих SMO. В 302 из них число материалов
совпадает; только эти ресурсы безопасно сопоставляются по ordinal без сдвига.
Получено 28 793 пары:

- 28 067 имеют одинаковое нормализованное собственное состояние;
- 726 различаются хотя бы одним значением;
- ID дочерних объектов и PC-only color controller исключены из own-state;
- номер legacy/current field исключён, если сами девять texture states равны.

Категории различий могут пересекаться:

| Категория | Пар |
|---|---:|
| render states | 491 |
| texture relationship | 457 |
| texture states | 428 |
| FinalBlendOp | 349 |
| pass count | 91 |
| UV-controller relationship | 40 |
| material colors без power | 20 |
| vertex alpha | 14 |
| static UV | 10 |
| specular power | 8 |

Это важный результат: PC и PS2 используют один serializer-контракт, но PS2 не
является простой побайтной копией PC-материалов. Часть render state и связей
намеренно адаптирована под платформу.

## Свидетельства executable

Оба executable содержат регистрации и serializer-имена:

- `spMaterial`, `spMaterialData`, `spMaterialSerializer`;
- `spMaterialDataSerializer`, платформенные `spDXMaterialDataSerializer` и
  `spPS2MaterialDataSerializer`;
- `spMaterialPassLayer`, `spMaterialTextureLayer`, `spStdLayer`;
- `esfMaterialRenderStates`, `esfMaterialVertexAlpha`, `esfMaterialColor`,
  `esfMaterialPass`, `esfMaterialLayer`, `esfMaterialColorController`;
- `esfMaterialLayerStaticUVTransform`, `esfMaterialLayerTexture`,
  `esfMaterialLayerAnimController`, `esfMaterialLayerUVController`,
  `esfMaterialLayerTextureStates`;
- `spTextureStateBlockOld` для legacy field `8`.

Runtime layout, ownership и deep-clone первых трёх layer-классов теперь
подтверждены отдельно в [карточке material layers](native-class-sp-material-layers.md).

Строки ошибок writer дополнительно называют getters для 11 render states,
`GetFinalBlendOp`, цветов/specular power, матрицы static UV, девяти texture
states и object relationships. Это независимое подтверждение семантики, а не
только интерпретация повторяющихся байтов.

В executable видны и ненаблюдаемые в concrete-корпусе возможности material
layer: camera/cubemap/movie/render-target texture sources и UV generation.
Точные serializer field ID этих путей ещё не доказаны, поэтому они не добавлены
в registry как известные поля.

## Реализация и воспроизведение

Viewer использует `SmoMaterialDataDecoder`: Object fields показывают номера
проходов, blend operation, имена layer class, оба state blocks, матрицу static
UV и разрешённые тип/ID/storage каждой связи. Decoder остаётся read-only.

Исследовательский отчёт воспроизводится командами:

```powershell
python research\analyze_smo_material_data.py `
  local-data\results\smo-corpus-v2.sqlite --summary --examples 0

dotnet run --project tools\SmoViewer\SmoViewer.Inspect -- `
  research-db analyze-class local-data\results\smo-corpus-v2.sqlite `
  spMaterialData --json
```

Analyzer заново читает directory/PCK bytes, строго декодирует 105 588 объектов,
проверяет ожидаемые варианты и PC/PS2-пары, сверяет serializer tokens обоих
executable и идемпотентно обновляет определения полей, варианты, назначения и
evidence.

## Открытые вопросы

1. Восстановить точные engine enum names для всех 11 + 9 state slots и всех
   значений `FinalBlendOp`, а не выводить роль только из consumer behavior.
2. Найти или сконструировать валидные примеры executable-only layer sources и
   доказать их field ID/layout.
3. Проверить mutation/repack в нативной игре: сначала изменение значения внутри
   существующего fixed-size поля, затем связи и число pass.
Связанный `spAnimTexController` после составления этого списка полностью разобран:
28 экземпляров трёх корпусов, три storage variants и 974 timed texture frames.
Открытым осталось только runtime-поведение sequence end/loop/clamp; оно включено
в [`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).
