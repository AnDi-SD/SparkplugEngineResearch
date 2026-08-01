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
| Где находятся transform и parent links? | объектный граф model/node/render node и runtime matrices | несколько составных моделей собираются без ручных поправок |
| Какие оси, handedness, winding и units? | известная сцена, face culling и transforms | правило единообразно воспроизводит ориентацию/масштаб |
| Что означают material state 0 и массивы 11+9? | D3D9 state call sites и controlled edits | состояния названы и preview совпадает с игрой на выбранных объектах |
| Как вычисляются FinalBlendOp/alpha? | прозрачные, additive и masked материалы | renderer воспроизводит эталонные кадры без per-file hacks |
| Как связаны material/layer/texture? | каталог и serializer references | все текстуры clean sample достигаются через graph, не signature scan |

## P2 — скелет и анимация

| Вопрос | Нужное evidence | Условие закрытия |
|---|---|---|
| Как устроены `spSkin`, bones и weights? | персонажи с mesh+skin и bind pose | корректная bind pose и проверка весов |
| Как связаны `ANM` и `SAN`? | пары state/resource и loader trace | документированный lookup и один проигрываемый clip |
| Где хранятся timing/interpolation? | несколько clips разной длины | воспроизводимая временная шкала без guessed constants |

## P3 — мир и gameplay layer

| Вопрос | Нужное evidence | Условие закрытия |
|---|---|---|
| Как `SPT` ссылается на SMO/компоненты? | parser ссылок и несколько шаблонов | dependency list совпадает с runtime loading |
| Как `SPL` размещает экземпляры? | level file, transforms, известные landmarks | минимальная сцена уровня совпадает с игрой |
| Как устроены collision/BV? | `spCollisionInfo`, `spMeshBV`, runtime queries | collision geometry визуализируется и согласуется с поверхностью |

## Метод работы

1. Сначала зафиксировать класс корпуса и SHA-256 manifest локально.
2. Сохранить команду, commit инструмента и агрегированный результат.
3. Для нового варианта получить минимальную строгую диагностику.
4. Добавить synthetic fixture, не содержащий игровых данных.
5. Реализовать decode и проверить отсутствие regressions.
6. Перенести подтверждённый итог в `docs/`, а ход эксперимента — в `journal/`.
