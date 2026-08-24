# Открытые вопросы

Приоритет означает ценность для следующего работающего среза, а не уверенность в гипотезе.

## P0 — границы строгого SMO parser

| Вопрос | Нужное evidence | Условие закрытия |
|---|---|---|
| Что означает primitive type `2`? | несколько pristine объектов, indices/vertex counts, runtime draw call | строгий decode + synthetic regression + корректная визуализация |
| Как устроен PS2 `E1` preamble? | минимальные примеры вариантов `8/9`, сравнение границ и executable loader | все 494 текущих случая классифицированы структурно без signature scan |
| Почему расходятся PS2 boundaries? | object tree и byte accounting вокруг 137 случаев | parser точно завершает каждый блок и следующий объект начинается ожидаемо |
| Что означают header `0x08` и `0x10`? | распределение по clean PC/PS2 corpus и чтение loader | значения имеют проверяемую семантику, не только корреляцию с именем файла |
| Какие stale offsets созданы старым repack? | pristine/modified пары и byte diff | детерминированный repair report и catalog-safe repack |

## P1 — корректный статический render

| Вопрос | Нужное evidence | Условие закрытия |
|---|---|---|
| Как декодируются все vertex layouts/FVF? | группировка layouts, D3D declarations, несколько visual checks | position/normal/color/UV/weights описаны для всего clean PC corpus |
| Какие serializer references являются transform parent links? | object graph, runtime matrices и partitioned levels | node/model chains, static world placements и non-spatial sector/portal containment различаются структурно |
| Какие оси, handedness, winding и units? | известная сцена, face culling и transforms | правило единообразно воспроизводит ориентацию/масштаб |
| Что означают material state 0 и массивы 11+9? | D3D9 state call sites и controlled edits | состояния названы и preview совпадает с игрой на выбранных объектах |
| Как вычисляются FinalBlendOp/alpha? | прозрачные, additive и masked материалы | renderer воспроизводит эталонные кадры без per-file hacks |
| Как связаны material/layer/texture? | каталог и serializer references | все текстуры clean sample достигаются через graph, не signature scan |

## P2 — скелет и анимация

| Вопрос | Нужное evidence | Условие закрытия |
|---|---|---|
| Какая runtime skinning formula и pose update используются? | controlled pose, inverse-bind и runtime trace | animated preview совпадает с игрой |
| Как exporter строит 16-slot PC palettes и разбивает triangles? | независимые rigs >16 bones и синтетический импорт | воспроизводимое совпадение chunk/palette boundaries |
| Почему runtime stride skinned E1 на 12 байт больше serialized stride? | loader/render trace для `0x097E`/`0x197E` | назначение дополнительных байтов подтверждено |
| Как связаны `ANM` и `SAN`? | пары state/resource и loader trace | документированный lookup и один проигрываемый clip |
| Где хранятся timing/interpolation? | несколько clips разной длины | воспроизводимая временная шкала без guessed constants |

## P3 — мир и gameplay layer

| Вопрос | Нужное evidence | Условие закрытия |
|---|---|---|
| Как `SPT` ссылается на SMO/компоненты? | parser ссылок и несколько шаблонов | dependency list совпадает с runtime loading |
| Как `SPL` размещает экземпляры? | level file, transforms, известные landmarks | минимальная сцена уровня совпадает с игрой |
| Как устроены collision/BV? | `spCollisionInfo`, `spMeshBV`, runtime queries | collision geometry визуализируется и согласуется с поверхностью |

## Параллельный трек — widescreen и GUI

Статическая карта нативного Resolution path, камеры и GUI зафиксирована в
[документации](../docs/engine/display-resolution-camera-gui.md). Следующие вопросы
требуют runtime-проверки и разбора menu `.smo`:

| Вопрос | Нужное evidence | Условие закрытия |
|---|---|---|
| Сохраняется ли Resolution index `3`? | выбор скрытого `1600x1200`, сохранение и повторная загрузка | индекс проходит UI/settings/apply path без повреждения состояния |
| Как безопасно добавить четвёртую подпись? | размер объекта меню, constructor/destructor и все обращения к массиву | строка `+0x29C` имеет подтверждённое владение и жизненный цикл |
| Какая камера является gameplay camera? | runtime trace camera setters в одной сцене | Hor+ применяется только к перспективным игровым камерам |
| Следует ли mouse hit-testing за GUI camera? | controlled change virtual width и клики по известным элементам | визуальные и интерактивные координаты совпадают на 4:3/16:9/21:9 |
| Как ведут себя FMV, loading screens и полноэкранные меню? | captures и trace render paths | для каждого класса выбран stretch, crop или pillarbox без регрессий |
| Корректен ли D3D Reset при смене width/height? | повторные переключения режимов и lifecycle trace | ресурсы пересоздаются без пропажи, crash и stale viewport |
| Как runtime связывает GUI anchors с текстом и hit-testing? | trace загрузки `igmenu_opt_pc.smo`, изменения `value_resolution` и клики по `GUICollision` | runtime-текст и интерактивные координаты связаны со статическими узлами без signature scan |
| Нужен ли предел safe-area на ultrawide? | сравнение HUD на 16:9, 21:9 и 32:9 | сформулировано проверяемое правило layout без чрезмерного разнесения HUD |

## Метод работы

1. Сначала зафиксировать класс корпуса и SHA-256 manifest локально.
2. Сохранить команду, commit инструмента и агрегированный результат.
3. Для нового варианта получить минимальную строгую диагностику.
4. Добавить synthetic fixture, не содержащий игровых данных.
5. Реализовать decode и проверить отсутствие regressions.
6. Перенести подтверждённый итог в `docs/`, а ход эксперимента — в `journal/`.
