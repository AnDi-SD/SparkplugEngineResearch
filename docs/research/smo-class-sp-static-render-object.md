# Полный разбор `spStaticRenderObject`

Дата проверки: 2026-08-28. Class ID: `0x56D67170`.

## Итог

`spStaticRenderObject` — world-space размещение одного статического `spModel`.
Во всех исследованных PC- и PS2-файлах объект имеет одну serializer-секцию и
ровно три смысловых поля. Строгий decoder повторно прочитал исходные
directory/PCK-ресурсы и принял все 61 851 уникальный объект:

| Корпус | Объектов | SMO-ресурсов | Размер объекта |
|---|---:|---:|---:|
| PC working | 20 469 | 29 | 209..350 768 |
| PC pristine | 20 469 | 29 | 209..350 768 |
| PS2 pristine | 20 913 | 30 | 209..294 769 |

Все объекты именованы. Разброс полного размера вызван вложенным деревом
`spModel`, а не подтипами самого `spStaticRenderObject`.

## Serializer layout

Фактический порядок отличается от числового порядка enum:

| Порядок | Field | Ключ | Payload |
|---:|---:|---|---|
| 1 | 1 | `static_render_object.transform` | 64-byte row-vector affine `Matrix4x4` |
| 2 | 2 | `static_render_object.inverse_transform` | 64-byte engine inverse `Matrix4x4` |
| 3 | 0 | `static_render_object.renderable` | inline relationship → `spModel` |
| 4 | 0 | terminator | пустой payload |

Каждый field 0 разрешается в ненулевой object ID ровно одного `spModel`.
Relationship всегда имеет inline-форму: после ID и размера находится полный
`SBOO`; размер совпадает с `SerializedSize` target, class ID равен
`0x763277DB`, а target является физическим ребёнком размещения. Размер
вложенной модели: PC `55..350 614`, PS2 `55..294 615` байт.

Физический родитель каждого размещения — `spPartitionNode`. Это containment,
а не дополнительный transform: готовая world matrix хранится непосредственно
в `spStaticRenderObject`.

## Доказательство из executable

Оба executable содержат `spStaticRenderObjectSerializer` и имена:

- `esrosfStaticRenderObjectTransform`;
- `esrosfStaticRenderObjectInvTransform`;
- `esrosfStaticRenderObjectRenderable`.

Диагностические строки serializer связывают первые два поля с
`GetWorldMatrix()` и `GetWorldInverseMatrix()`. Для renderable видны
`GetRenderable(i)`, `IndexRelationship` и `SerializeRelationship`. Значит,
runtime API допускает список, хотя во всём доступном корпусе сериализован ровно
один элемент.

## Необычный `InvTransform`

Название поля вводит в заблуждение для масштабированных объектов. Пусть world
matrix использует row-vector convention, её верхний блок — `A`, а translation
в последней строке — `T`. Sparkplug хранит:

```text
InvTransform.linear      = transpose(A)
InvTransform.translation = -T * transpose(A)
```

Это обычный inverse только для ортонормированного базиса с единичным масштабом.
Если есть scale, произведение `Transform * InvTransform` содержит квадрат
масштаба. Все 61 851 объекта точно следуют engine-формуле с максимальным
отклонением сериализованных `float` менее `0.001`; ни одна матрица не singular.

| Корпус | Единичный scale | Равномерный scale | Mirrored | Математический inverse |
|---|---:|---:|---:|---:|
| каждый PC | 8 511 | 14 127 | 1 008 | 6 785 |
| PS2 | 8 728 | 14 571 | 1 006 | 6 986 |

Диапазон длин базисных осей — `0.006246..8.061983`. Оси при этом попарно
ортогональны: максимальный скалярный продукт около `1.52e-6`. Самый наглядный
контрпример — `treasure_rock_03` в `levels/domino/domino04.smo`: длины осей
около `8.06`, `2.32`, `3.44`, поэтому общий математический inverse и поле 2
существенно различаются.

Это точное правило shipped-корпуса, но не доказанная граница runtime. Рабочий
`Alfea02.smo`, созданный ранним Level Creator, хранит для масштабированных
размещений результат `Matrix4x4.Invert()` и при этом корректно отображается
игрой. Пока это доказывает только совместимость итогового файла. Loader может
читать field 2 как второй допустимый вариант, игнорировать его, пересчитывать
либо передавать consumer, который не участвует в обычном рендере.

Строгий decoder классифицирует каноническую и математическую формы для
совместимости существующих проектов и отклоняет матрицу, не соответствующую ни
одной из них. Это правило нашего reader, а не восстановленный контракт игры.
Writer продолжает создавать каноническую transpose-basis пару и меняет обе
матрицы одной транзакцией. Runtime-семантика будет закрыта только после трассы
serializer и последующих чтений обоих полей в executable.

## Варианты и межплатформенное сравнение

В базе назначен один вариант `static_render_object_inline_model` всем 61 851
объектам. Тысячи различных `field_shape` по размеру relationship не являются
подвидами: это размеры вложенных model/mesh/material/texture-деревьев.

Все 29 общих ресурсов PC working/pristine полностью совпадают. Между PC
pristine и PS2 найдено 29 общих путей; в 26 совпадает число размещений, что даёт
18 390 ordinal-пар:

| Проверка | Совпало | Отличалось |
|---|---:|---:|
| имя `spStaticRenderObject` | 18 390 | 0 |
| имя вложенного `spModel` | 18 390 | 0 |
| translation | 18 390 | 0 |
| `Transform` | 18 388 | 2 |
| `InvTransform` | 18 388 | 2 |

Обе матричные разницы находятся в `levels/alfea/alfea_broken_02.smo`:
`doorFRVS09` и `doortop11`. Translation совпадает; PC хранит масштаб/зеркало,
а PS2 — единичный базис. Это платформенные данные, не ошибка parser-а.

## Реализация и воспроизведение

`SmoStaticRenderObjectDecoder` требует точный порядок полей, affine-матрицы,
одну из двух подтверждённых форм inverse, разрешённую связь с `spModel` и
корректную inline-топологию.
Viewer показывает все три поля как decoded values: translation, три базисные
оси с длинами и target relationship.

```powershell
python research\analyze_smo_static_render_object.py `
  local-data\results\smo-corpus-v2.sqlite

dotnet run --project tools\SmoViewer\SmoViewer.Inspect -- `
  research-db analyze-class local-data\results\smo-corpus-v2.sqlite `
  spStaticRenderObject --json
```

Analyzer идемпотентно обновляет три field definition, decoded values,
единственный variant, 61 851 assignment и четыре evidence-записи.

## Открытые вопросы

1. Проверить в PC и PS2 игре сохранение transform с неравномерным scale после
   полного load/save цикла.
2. Выяснить, может ли executable-only путь реально сериализовать несколько
   `GetRenderable(i)` или reference-only relationship: в корпусе их нет.
3. Не разрешать замену target/model relationship, пока не реализована
   безопасная перестройка inline object tree и object directory.

`spSkin` и collision-классы после составления первоначальной очереди разобраны.
Ближайшая проверка этого класса — controlled nonuniform-scale load и
length-preserving transform test по
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

## PC runtime — 2026-09-06

[Native checkpoint](native-pc-partition-runtime.md) подтвердил exact10C,
NamedObject+support14, Scene88/world8C/inverseCC. Обе матрицы ctor копирует из
shared identity после **отдельного CRT6D38C0**; cold PE zeros не default игры.
Actual44FBA0 передаёт две сохранённые матрицы в renderer независимо, без
recompute на этой границе. Это сужает inverse-вопрос выше, но не доказывает
whole serializer/upstream/backend contract и принятие любого input.
Model refs/append/bounds, name-only clone и draw failure policy исполнены:
matrix false ignored by draw, Model false stops. No GPU/game/source class claim.
