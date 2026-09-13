# spStaticRenderObject

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spModel](../../../Sparkplug/Code/Sparkplug/spModel.h), [spStaticRenderObject](../../../Sparkplug/Code/Sparkplug/spStaticRenderObject.h).

## Итог

Все объекты именованы. Разброс полного размера вызван вложенным деревом
`spModel`, а не подтипами самого `spStaticRenderObject`.

## Serializer layout

Фактический порядок отличается от числового порядка enum:

| Порядок | Field | Ключ | Payload |
| ---: | ---: | --- | --- |
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

Оба executable содержат `spStaticRenderObjectSerializer` и имена:

- `esrosfStaticRenderObjectTransform`;
- `esrosfStaticRenderObjectInvTransform`;
- `esrosfStaticRenderObjectRenderable`.

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

| Единичный scale | Равномерный scale | Mirrored | Математический inverse |
| ---: | ---: | ---: | ---: |
| 8 511 | 14 127 | 1 008 | 6 785 |
| 8 728 | 14 571 | 1 006 | 6 986 |

Диапазон длин базисных осей — `0.006246..8.061983`. Оси при этом попарно
ортогональны: максимальный скалярный продукт около `1.52e-6`. Самый наглядный
контрпример — `treasure_rock_03` в `levels/domino/domino04.smo`: длины осей
около `8.06`, `2.32`, `3.44`, поэтому общий математический inverse и поле 2
существенно различаются.

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

| Совпало | Отличалось |
| ---: | ---: |
| 18 390 | 0 |
| 18 390 | 0 |
| 18 390 | 0 |
| 18 388 | 2 |
| 18 388 | 2 |

Обе матричные разницы находятся в `levels/alfea/alfea_broken_02.smo`:
`doorFRVS09` и `doortop11`. Translation совпадает; PC хранит масштаб/зеркало,
а PS2 — единичный базис. Это платформенные данные, не ошибка parser-а.

`SmoStaticRenderObjectDecoder` требует точный порядок полей, affine-матрицы,
одну из двух подтверждённых форм inverse, разрешённую связь с `spModel` и
корректную inline-топологию.
Viewer показывает все три поля как decoded values: translation, три базисные
оси с длинами и target relationship.
