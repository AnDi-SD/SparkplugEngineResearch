# `spZonePortalNode` (`0xABB5AB2C`)

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

## Покрытие корпуса

| Корпус | Объекты | SMO | Position + Animated | + Static | + Rotation |
|---|---:|---:|---:|---:|---:|
| PC working | 103 | 18 | 80 | 22 | 1 |
| PC pristine | 103 | 18 | 80 | 22 | 1 |
| PS2 pristine | 103 | 18 | 80 | 22 | 1 |

Размеры сериализованных объектов для трёх форм равны 52, 54 и 70 байтам.
Это один serializer variant: различаются только optional-поля унаследованного
`spNode`, а не подтипы `spZonePortalNode`.

`Static=true` встречается в четырёх группах ресурсов на корпус: восемь узлов в
`Alfea02`, два в `Alfea_broken_02`, восемь в `Alfea_night_02` и четыре в
`Domino04`. Единственный Rotation находится у `portal_01` в
`Cloud01/cloud01_02.smo` и равен `(0.5, 0.5, 0.5, -0.5)`. `Animated` во всех
309 объектах явно равен `false`.

## Порядок и графовая семантика

Во всех 309 объектах порядок двух ссылок одинаков:

1. портал с суффиксом `BackToFront`;
2. портал с суффиксом `FrontToBack`.

Для каждой пары destination первого равен source zone второго и наоборот. Оба
портала открыты, содержат по четыре вершины, а winding второго polygon является
точным обращением первого. Каждый из 618 `spZonePortal` принадлежит ровно одной
такой паре. Поэтому порядок повторяемого field 0 семантически значим и не должен
сортироваться Viewer или будущим writer.

## Position не заменяет polygon

В каждом корпусе встречается 65 различных Position. Только у 69 из 103 узлов
Position совпадает с центроидом первого polygon с точностью `0.001`; у остальных
34 отличие реально и иногда превышает сотни единиц. Следовательно, Position —
самостоятельное унаследованное placement-поле, а не вычисляемый центр портала.
Редактор должен сохранять и transform узла, и обе portal-ссылки независимо.

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

- PC working и PC pristine совпадают байт-в-байт для 103 из 103 объектов.
- Все 103 PC/PS2-пары совпадают по Position, optional flags и упорядоченным
  целевым порталам.
- Сырые bytes совпадают у 88 пар; в остальных 15 различаются только два object
  ID portal-ссылок.
- Analyzer зарегистрировал 10 field definitions, один variant, 309 assignments,
  1 305 field annotations и четыре evidence-записи.

Строгий decoder находится в `SmoZonePortalNodeDecoder`; Inspector показывает
унаследованные node-поля и обе именованные portal-ссылки, не интерпретируя их
как inline-геометрию.

## Воспроизведение

```powershell
dotnet tools/SmoViewer/SmoViewer.Inspect/bin/Debug/net8.0/SmoViewer.Inspect.dll `
  research-db analyze-class local-data/results/smo-corpus-v2.sqlite `
  spZonePortalNode --json

python research/analyze_smo_zone_portal_node.py `
  local-data/results/smo-corpus-v2.sqlite
```

Связанные [`spBSPNode`](smo-class-sp-bsp-node.md),
[`spOcclusionVolume`](smo-class-sp-occlusion-volume.md) и
[`spMeshNavigationSet`](smo-class-sp-mesh-navigation-set.md) имеют отдельные
wire-карточки; это не доказательство полного runtime-разбора всех трёх классов.
`Open`/navigation/culling в настоящей игре требуют отдельной проверки.

## PC runtime дополнение — 2026-09-06

[Executable/runtime карточка](native-pc-zone-portal-runtime.md): actualC4,
Node-only clone и borrowed duplicate-preserving vector. Native append481930
не ограничивает пару и принимает null ниже serializer gate. Inherited Node
world/Enabled не меняют ни polygon/plane, ни Open. Partial original-named
source и native whole Scene portal tests63 закрывают эти конкретные contracts,
но не весь Scene/loader, game Open controller или near-plane45E870 branch.
