# PC render-узел: сцена, world cache и граница отрисовки

## Размер, две таблицы и lifetime

Original factory `425520→13C5390` выделяет **1D4**; ctor `424F60→13D8030`
вызывает Node, затем support constructor `469E00→13E0CB0`. Primary `6DCAA4`
содержит14 callable slots, secondary `6DCADC` —6. Прежние «20 primary» были
ошибкой нашей интерпретации соседних таблиц, не повреждением executable.

| PC offset | Подтверждённая роль/default |
| ---: | --- |
| B4 / B8 / BC-C4 | support vptr; untouched allocator; owned renderable vector |
| C8 / D8 | local/world sphere: center3+radius, initially zero |
| E8 / EC | pointers to own matrices138/178 |
| F0-117 | light-cache support, initially zero; полный тип/внутренние поля открыты |
| 118 / 11C | zero words;118 сравнивается с scene40, original roles не названы |
| 120-123 | bytes00,01,01,01;123 разрешает light-manager update |
| 124 / 128 / 12C | complete-object self / previous / next render-node in scene |
| 130 / 131-133 / 134 | cull bypass0 / untouched padding / matrix dirty0 |
| 138 / 178 / 1B8 | identity world/inverse matrices / reciprocal world scale(1,1,1) |
| 1C4 / 1C8-1D0 | untouched allocator / borrowed callback pointer vector |

## World, bounds и lazy matrices

Tiny incoming radius<.001 игнорируется; containing sphere заменяет/сохраняет результат; near-coincident center comparison покоординатный. Helper преобразует sphere **старой cached matrix138**, измеряя длину преобразованного X-radius.

Поэтому bounds-only и transform-dirty paths **не эквивалентны** при nonuniform scale. Если scene существует, byte123 и Enabled200 разрешают46AC40 light-cache rebuild;424EF0 обновляет partition membership, кроме skybox/self-partition/disabled cases. Последние nonempty manager callbacks — ещё внешняя граница.

## Cull, direct draw и queue

Secondary424B60 получает `(camera,forceVisible)` и complete this+B4:

- !Enabled200 или culled →success без matrix/draw;
- rendererC050=0: собственный support v4 готовит matrices, failure останавливает;
  каждый renderable v24 получает `(camera,support)`, его return игнорируется;
- rendererC050!=0: каждый идёт в456310 `(renderable,support,camera)`; первый false
  останавливает проход; immediate matrix setup здесь не вызывается.
