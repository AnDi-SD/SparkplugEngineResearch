# spAnimation / spAnimationSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spAnimation](../../../Sparkplug/Code/Sparkplug/spAnimation.h), [spAnimationSerializer](../../../Sparkplug/Code/Sparkplug/spAnimationSerializer.h).

## Идентичность и виртуальные таблицы

`spAnimation` имеет class ID `0x56EE563A`; engine RTTI объявляет direct base
`spController` (`0x4FAD24F1`). Registration initializer начинается по `0x006D1C10`
(`0x006D1C30` — callsite внутри него), объект
регистрации — `0x0075D248`, factory — защищённый entry `0x0041A090`.
Семислотовая vtable по `0x006DE6CC` содержит deleting destructor
`0x00430380`, clone `0x0041AA70`, inherited **name-copy** slot `0x00413120`,
registration getter `0x00430280` и два общих base slots.

Здесь обнаружено важное расхождение двух иерархий. `spController` и
`spSubController` имеют восьмислотовые vtable с дополнительным pure/abstract
slot `0x0060DB76`, а `spAnimation` — только семь слотов; его destructor в конце
вызывает PC body `0x00413090` — destructor **`spNamedObject`**, затем root
`0x004102B0`. Constructor теперь независимо подтверждает тот же physical base.
Предыдущее отнесение `0x00413090` к root исправлено. Engine RTTI relation
моделируется отдельно от физического `spNamedObject` prefix.

`spAnimationSerializer` имеет ID `0xC0ACBFA6`, direct base `spSerializer`
(`0x42429877`) и target class `spAnimation`. Registration находится по
`0x006D2FC0`/`0x0075EC48`, protected factory — `0x0043DAB0`. Primary vtable
расположена по `0x006E0B0C`, stream table из writer/index/reader — по
`0x006E0B00`. Точный original translation unit:
`Z:\Sparkplug\Code\Sparkplug\spAnimationSerializer.cpp`.

## Подтверждённый runtime storage

| Offset | Наблюдение |
| ---: | --- |
| `+0x10` | inherited shared name entry |
| `+0x14` | total animation time, совпадает с field `0` reader-а |
| `+0x18` | источник старшего байта actor input priority; analytical priority group, default0 |
| `+0x1C` | массив animation tracks |
| `+0x20` | число tracks |
| `+0x24` | ёмкость track array |
| `+0x2C/+0x30/+0x34` | begin/end/capacity массива owned tag pointers |
| `+0x38..+0x4C` | шесть общих values buffers: scalar linear/cubic, Vector3 linear/cubic, quaternion linear/cubic |
| `+0x50` | общий float times buffer |
| `+0x54` | значение cycle table `spDebugManager`, не постоянный default |
| `+0x58..+0x83` | pool descriptors размером `0x10`; exact original type ещё не назван |

Tag insertion `0x004305F0` держит owned tag pointers отсортированными по
`float` в tag `+0x14`. Original имя tag-типа пока не закреплено.

## Serializer и SAN

Writer `0x0043DFE0` и reader `0x0043ECC0` поддерживают поля `0..12` и
расширенный необязательный reserve field `64` (не terminator). Для PC SAN
строгий полезный минимум остаётся прежним:

- field `0` — total time;
- fields `2/3/4` — position, rotation и scale keys;
- field `1` — имя текущего track и его завершение;
- каждый key payload содержит key representation, count, times и values.

Ближайший runtime-потребитель треков теперь описан отдельно:
[`spTransformTrackEval`](sp-transform-track-eval.md).
