# spPartitionRenderable

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPartitionRenderable](../../../Sparkplug/Code/Sparkplug/spPartitionRenderable.h).

## Назначение

`spPartitionRenderable` — лист spatial-partition дерева, который группирует
одну или несколько моделей. Сам объект не хранит transform и не является
размещением сцены: его родитель всегда `spPartitionNode`, а каждая модель уже
содержит собственное renderable-поддерево. Поэтому размеры экземпляров от 228
до 559 239 байт отражают размеры вложенных `spModel`, а не сотни собственных
полей или подвидов `spPartitionRenderable`.

```text
SBOO spPartitionRenderable
field 1: UInt32 DebugColor (ARGB)
repeat 1..67 times:
    field 0: UInt32 objectId
             UInt32 inlineSerializedSize
             inline SBOO spModel
field 0: empty section terminator
```

Relationship payload имеет обычный Sparkplug inline-layout: размер после
первых восьми байт равен `inlineSerializedSize`, затем идут type hash и `SBOO`
целевого объекта. Во всех 19 989 случаях ID ненулевой, target однозначно
разрешается в `spModel`, совпадает с физическим child текущего
`spPartitionRenderable` и хранится inline. ID-only, sized-reference, null и
targets других классов не обнаружены.

## PC/PS2 сравнение

Две PC-копии совпадают побайтно для всех 2 324 объектов во всех 29 общих
ресурсах. Для 29 общих PC/PS2 ресурсов совпадает число
`spPartitionRenderable`; по same-path/ordinal сопоставлены 2 324 объекта:

- `DebugColor` совпадает у 2 324 из 2 324;
- число renderables совпадает у 2 323 из 2 324;
- полная упорядоченная последовательность имён `spModel` совпадает у 2 323 из
  2 324.

Единственное отличие — `Levels/Alfea/Alfea03.smo`, ordinal 1. После 17 общих
моделей PS2 добавляет `light_ray-000` и `detach ray-000`. Это различие состава
уровня, а не платформенный вариант serializer: encoding и layout остаются
общими.
