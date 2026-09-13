# spZonePortalNode

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spZonePortalNode](../../../Sparkplug/Code/Sparkplug/spZonePortalNode.h).

Статус: **полный структурный/read-only decode PC и PS2**. Запись и мутация
полей пока намеренно не разрешены.

## Итог

`spZonePortalNode` — не самостоятельный геометрический портал. Это
унаследованный от `spNode` размещённый узел, который объединяет два направленных
`spZonePortal` одной двусторонней связи. Собственная секция содержит только две
ссылки; polygon, destination zone и open flag хранятся в целевых порталах.

Наблюдаемый layout:

```text
spNode section
field 0: Position                 # Vector3, обязателен
[field 1: Rotation]               # Quaternion, optional
[field 4: Static]                 # Boolean=true, optional
field 8: Animated=false           # Boolean, обязателен
terminator

spZonePortalNode section
field 0: ZonePortal               # sized reference -> *BackToFront
field 0: ZonePortal               # sized reference -> *FrontToBack
terminator
```

Обе ссылки ненулевые, различны, имеют восьмибайтовую reference-форму и указывают
на `spZonePortal`, принадлежащие другим объектам. Сам `spZonePortalNode` всегда
является физическим ребёнком `spPartitionSystem` и ровно один раз присутствует
среди его унаследованных node children.

## Порядок и графовая семантика

Во всех 309 объектах порядок двух ссылок одинаков:

1. портал с суффиксом `BackToFront`;
2. портал с суффиксом `FrontToBack`.

Для каждой пары destination первого равен source zone второго и наоборот. Оба
портала открыты, содержат по четыре вершины, а winding второго polygon является
точным обращением первого. Каждый из 618 `spZonePortal` принадлежит ровно одной
такой паре. Поэтому порядок повторяемого field 0 семантически значим и не должен
сортироваться Viewer или будущим writer.

## Исполняемые файлы

PC serializer восстановлен в диапазоне `0x0044E5F0..0x0044EB52`: index
`0x0044E5F0..0x0044E663`, reader `0x0044E670..0x0044E811`, writer
`0x0044E820..0x0044EB52`. Первая секция делегируется `spNodeSerializer`, во
второй повторяется только `ezpnsfZonePortalNodeZonePortal`. Runtime vector
порталов хранит begin/end по `+0xB8/+0xBC`; null relationship отклоняется.

Независимый PS2 MIPS serializer находится в `0x001A2CC0..0x001A3100`: reader
`0x001A2CC0..0x001A2E60`, index `0x001A2E70..0x001A2F28`, writer
`0x001A2F30..0x001A3100`. Он создаёт target class `0x6523AC37`
(`spZonePortal`), повторяет тот же field 0 и использует runtime array/count по
`+0xC0/+0xC4`.

Executable допускает повторение field 0 как контейнерную операцию. Строгая
cardinality `2` является подтверждённым инвариантом всех доступных ресурсов,
поэтому decoder применяет её для раннего обнаружения повреждённых или пока
неизвестных вариантов.

## PC/PS2 и база исследований

Строгий decoder находится в `SmoZonePortalNodeDecoder`; Inspector показывает
унаследованные node-поля и обе именованные portal-ссылки, не интерпретируя их
как inline-геометрию.

## PC runtime дополнение

[Executable/runtime карточка](../../engine/visibility/zone-portal-runtime.md): actualC4,
Node-only clone и borrowed duplicate-preserving vector. Native append481930
не ограничивает пару и принимает null ниже serializer gate. Inherited Node
world/Enabled не меняют ни polygon/plane, ни Open. Partial original-named
source и native whole Scene portal tests63 закрывают эти конкретные contracts,
но не весь Scene/loader, game Open controller или near-plane45E870 branch.
