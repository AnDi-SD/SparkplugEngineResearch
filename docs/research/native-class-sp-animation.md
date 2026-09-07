# Нативные `spAnimation` и `spAnimationSerializer`

Дата PC-разведки: 2026-09-05. Статус: substantial wire/binding и partial
native runtime; PS2 отложен.

6 сентября [общий PC loader](native-pc-smo-san-loader.md) проверен на `bbush.san`
целиком с повторной загрузкой и настоящим реестром имён. 227 значений его
результата совпали с portable field reader/owned bindings/PRS. Полный native
startup, writer и оставшиеся варианты/отказы не объявляются завершёнными.

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

Protected factory и constructor теперь пройдены в bounded guest: exact size
`0x84`. Полная таблица и границы — в
[object lifecycle checkpoint](native-pc-animation-lifecycle.md).

| Offset | Наблюдение |
|---:|---|
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

Native track — оригинальный **`spAnimTrack`**, exact `0x44`, base `spTrack` (`0x14`).
Resize body `0x00430010` умножает capacity на `0x44` и переносит массив.
При shrink он разрушает хвост по capacity, не count: неинициализированный reserve
tail — отдельная опасная граница. Внутри track `+0x10` ведёт к имени, а `+0x14` содержит runtime
binding slot с sentinel `0xFFFFFFFF`; destructor снимает зарегистрированную
связь перед разрушением track. Это согласуется с уже наблюдавшимся в игре
exact-name binding и значениями slot из debugger probe.

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

Writer helper `0x0043DDC0` подтверждает разные размеры Vector3/quaternion и
разные key encodings, а не единственный жёстко заданный массив. Reader создаёт
track по необходимости, читает три PRS-ветви и регистрирует track только после
успешного имени/содержимого. Field `5` — tags; fields `6..12` задают размеры
семи общих массивов. Их точная таблица, representations `1..4`, reader attach,
cubic preparation и sampling теперь разобраны в
[PC SAN keys](native-pc-animation-keys.md). Field `64` отсутствует в pristine
`barrel.san`: это допустимо, а не ошибка файла.

[Full PC reader checkpoint](native-pc-san-reader.md) уточнил `Resize(hint + 1)`
для field `64`, exact serializer `0x4C` и fourteen reset scratch counters.
Оригинальный reader и tag name path прошли четыре SAN; переносимый
`spAnimationSerializer` использует существующий stream/block core.

Текущий viewer decoder не является полным native serializer replacement:
помимо tags/rollback, он не поддерживает cubic payload, реально встреченный
в scale track pristine `bbush.san`. Реконструкция и evidence сохранены отдельно;
код приложения в этом цикле не изменялся.

## Воспроизведение и остаток

```powershell
python research\inspect_animation.py
```

Проверка привязана к pristine PC SHA-256 и фиксирует vtable animation,
serializer и двух controller-base типов, девять
ключевых function bodies, track stride/ownership, target ID, extended field и
source path. PS2 в текущем цикле не использовался.

Следующие точные шаги: full resource-loader integration, name-binding registry,
остальные malformed/partial paths и actor/frame lifetime. Native reader rollback
не выполняет; header failure способен вернуть success. Constructor defaults,
track identity и normal tag/descriptor ownership теперь закрыты отдельным checkpoint.
Вычислительное ребро keys → evaluator → node controller → world cache уже
соединено в portable tests; frame/skin integration остаётся отдельной границей.

Ближайший runtime-потребитель треков теперь описан отдельно:
[`spTransformTrackEval`](native-class-sp-transform-track-eval.md).

Часовой PC-проход 2026-09-05 связал track array с playback entries `spActor`
и node controllers; подтвердил memory-layout key descriptors, PRS caches и
linear-position sampling. Полученный corner case двух ключей зафиксирован
без предположения о баге игры. На момент часового checkpoint key decoder,
поля `5..12`, tags и constructor defaults оставались открыты. См.
[PC animation runtime pipeline](native-pc-animation-runtime.md).

Продолжение закрыло reader-to-sampler representations `1..4`, shared-array
hints `6..12`, PRS math и cubic preparation: 396 guest checks, 120 сравнений
portable/native PRS+cache, 60 portable assertions. Последующий
[object lifecycle](native-pc-animation-lifecycle.md) добавил три original classes,
exact layouts, protected factory, name-only clone и normal ownership evidence.
На момент этого checkpoint полный loader/registry и malformed-tag transaction
ещё не были восстановлены. Более поздний generic loader/registry evidence
описан в начале карточки; оставшиеся malformed transactions открыты.
Полный field reader, normal tag-name path и ошибки header/total-time отдельно
подтверждены [следующим checkpoint](native-pc-san-reader.md); writer ещё не перенесён.

Ночной [actor binding checkpoint](native-pc-actor-binding.md) исполнил start:
`(animation18 << 24) | (managerFrame & 0x00FFFFFF)` даёт input priority.
Поле18 больше не wholly unknown; имя и upstream setter ещё не найдены.
Portable accessor добавлен, clone по-прежнему копирует только имя.
Original SAN/shared registry lifetime проверен manager checkpoint. После него
добавлен owned-binding reader (старый borrowed resolver сохранён отдельно).
Новый [PC writer checkpoint](native-pc-san-writer.md) исследует полную запись
SAN fields, но не объявляет завершёнными FFPS/FAT save и lossless unknowns.
