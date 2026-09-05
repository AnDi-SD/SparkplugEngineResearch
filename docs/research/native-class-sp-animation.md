# Нативные `spAnimation` и `spAnimationSerializer`

Дата PC-разведки: 2026-09-05. Статус: substantial wire/binding и partial
native runtime; PS2 отложен.

## Идентичность и виртуальные таблицы

`spAnimation` имеет class ID `0x56EE563A`; engine RTTI объявляет direct base
`spController` (`0x4FAD24F1`). Registration initializer находится по `0x006D1C30`, объект
регистрации — `0x0075D248`, factory — защищённый entry `0x0041A090`.
Семислотовая vtable по `0x006DE6CC` содержит deleting destructor
`0x00430380`, clone `0x0041AA70`, inherited/no-op copy slot `0x00413120`,
registration getter `0x00430280` и два общих base slots.

Здесь обнаружено важное расхождение двух иерархий. `spController` и
`spSubController` имеют восьмислотовые vtable с дополнительным pure/abstract
slot `0x0060DB76`, а `spAnimation` — только семь слотов; его destructor в конце
вызывает PC body `0x00413090`, уже относимый к `spBaseObject`, а не destructor
`spController`. Следовательно, engine RTTI base нельзя автоматически считать
физическим C++ base: для portable layout `spAnimation` пока следует опираться
на `spBaseObject` prefix и отдельно моделировать engine type relation.

`spAnimationSerializer` имеет ID `0xC0ACBFA6`, direct base `spSerializer`
(`0x4242AD77`) и target class `spAnimation`. Registration находится по
`0x006D2FC0`/`0x0075EC48`, protected factory — `0x0043DAB0`. Primary vtable
расположена по `0x006E0B0C`, stream table из writer/index/reader — по
`0x006E0B00`. Точный original translation unit:
`Z:\Sparkplug\Code\Sparkplug\spAnimationSerializer.cpp`.

## Подтверждённый runtime storage

Полный размер объекта пока нельзя заявить из-за защищённого factory, но
destructor `0x00430130` и helper-ы дают непрерывный наблюдаемый префикс:

| Offset | Наблюдение |
|---:|---|
| `+0x14` | total animation time, совпадает с field `0` reader-а |
| `+0x1C` | массив animation tracks |
| `+0x20` | число tracks |
| `+0x24` | ёмкость track array |
| `+0x2C/+0x30/+0x34` | begin/end/capacity массива owned tag pointers |
| `+0x38..+0x4C` | шесть отдельно выделяемых auxiliary key/time buffers |
| `+0x50` | ещё один отдельно выделяемый buffer/count payload |
| `+0x58` | начало контейнерного состояния, exact type ещё не назван |

Размер одного native track равен `0x44`: resize body `0x00430010` умножает
capacity на `0x44`, переносит этот массив и корректно разрушает отброшенный
хвост. Внутри track поле `+0x10` ведёт к имени, а `+0x14` содержит runtime
binding slot с sentinel `0xFFFFFFFF`; destructor снимает зарегистрированную
связь перед разрушением track. Это согласуется с уже наблюдавшимся в игре
exact-name binding и значениями slot из debugger probe.

Tag insertion `0x004305F0` держит owned tag pointers отсортированными по
`float` в tag `+0x14`. Original имя tag-типа пока не закреплено.

## Serializer и SAN

Writer `0x0043DFE0` и reader `0x0043ECC0` поддерживают поля `0..12` и
расширенный terminator/control field `64`. Для реально встреченного PC SAN
строгий полезный минимум остаётся прежним:

- field `0` — total time;
- fields `2/3/4` — position, rotation и scale keys;
- field `1` — имя текущего track и его завершение;
- каждый key payload содержит key representation, count, times и values.

Writer helper `0x0043DDC0` подтверждает разные размеры Vector3/quaternion и
разные key encodings, а не единственный жёстко заданный массив. Reader создаёт
track по необходимости, читает три PRS-ветви и регистрирует track только после
успешного имени/содержимого. Поля `5..12` относятся к tags и общим time-key
таблицам; их точные original типы и редкие wire-варианты ещё не закрыты.

Ключевое ограничение текущего viewer decoder: он корректно читает наблюдаемые
поля `0..4`, но не должен называться полным native serializer replacement до
проверки полей `5..12`, всех key encodings и rollback на частично прочитанном
track.

## Воспроизведение и остаток

```powershell
python research\inspect_animation.py
```

Проверка привязана к pristine PC SHA-256 и фиксирует vtable animation,
serializer и двух controller-base типов, девять
ключевых function bodies, track stride/ownership, target ID, extended field и
source path. PS2 в текущем цикле не использовался.

Следующие точные шаги: восстановить `spController`/`spSubController` prefix,
закрыть constructor defaults, назвать структуры track/tag/time-key по callers,
затем соединить update/evaluate с `spNode` transform и уже доказанным
`spSkin` palette consumer.

Ближайший runtime-потребитель треков теперь описан отдельно:
[`spTransformTrackEval`](native-class-sp-transform-track-eval.md).
