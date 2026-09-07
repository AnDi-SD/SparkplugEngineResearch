# PC SAN actor → прочитанная кость → Skin palette/draw (CP66)

2026-09-07, pristine WinxClub.exe
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Файл `Media/Animations/bbush.san` SHA-256
`706BD0E5C70111BBD7C9524B3A37B1B2D867EC86FDC5A7A7908FAC8BBFCA428E`.

Расширен [связанный read/render CP65](native-pc-skin-loaded-render.md).
Тот же Node, реально прочитанный Skin serializer, получает имя `Plane01`,
затем привязывается настоящим actor discovery к первому треку SAN. Нативный
`43ECC0` читает все пять треков и готовит исходные key pools; FFPS envelope
предварительно проверен и object payload выбран внешним bounded fixture.
Whole SAN FAT/file loader в этой последовательности не заявляется.

Выполнены original factories manager `454640`, actor `5A3620`, animation
`41A090`, serializer `43DAB0`; name registry, discovery `5A33F0`, start
`5A1E30`, manager frame `4535A0` → actor tick `5A2380` → track sampling
`479290` → controller application. После явного world update `421420(1)`
полный Skin `46A240` доводит эту кость до palette, BlendMatrices constants
и indexed draw. Не подменены actor/evaluator/controller/Node/render bodies.

## Сравнение

Проверены отдельные шаги `0.25`, `0.5`, `0.75`, `1.25` секунды. Последний
проходит loop boundary и получает sample time0.25. Native events/flush
записываются как внешний queue contract; полноценный dispatcher/reentry
остаётся отдельным открытым участком.

Source harness читает те же Skin bytes и настоящий `bbush.san`, использует
owned SAN name leases, `DiscoverNodeForAnalysis`, `StartForAnalysis` и
**настоящий** `AdvanceFrameForAnalysis` с dispatch в actor. Сверяются raw
float words sample time и всех local/world PRS, frame counter, binding count,
Node flags, затем полная Skin palette и каждый device event. Все четыре
capture совпали **побитово**, без float tolerance для этого набора.

Например, в0.25 исходная cubic scale даёт
`{0.7766315937042236, 0.9197549819946289, 0.7766315340995789}`: небольшое
различие X/Z сохранено. С inverse-bind translation `{5,6,7}` это достигает
разных palette translation и shader constant rows; результат не заменён
identity или заранее вычисленными Skin input matrices.

`pc-skin-san-render`: **4 exact captures,129 native assertions** (32/32/32/33).
Skin read50702, SAN read39170, manager tick4692/4797/4857/4745, render15288
инструкций на завершённый call. Peak64896 bytes в прежней64KiB arena;
в каждом сценарии75 owner generations освобождены ровно один раз.

## Владение и границы

Native Skin заимствует кость. Стенд явно держит один intrusive reference;
actual actor discovery увеличивает его до2, destructor возвращает1. Имя —
объявленная внешняя named-object ownership boundary, теперь использующая
тот же generation-aware allocator; replacement освобождает прежнее имя.
Source удерживает Node через shared_ptr, после уничтожения actor/animation
имя остаётся у Node, а name registry пуст. Указатели и animated cache не
перемещаются при повторном использовании освобождённой arena.

Actor, animations, registry и временные caller engine/request inputs
освобождаются **между завершёнными фазами**, до подготовки renderer. Это
сохранённый граф через последовательность API, не одновременно живой full
engine frame. Обновление world cache вызвано явно; внешний frame hook ещё
не доказан. Mesh/material/cache/device inputs сохраняют границы CP64.
Лимиты100000 instructions/2s/call,30s/child,64KiB arena,32KiB request не менялись.

Новые крайние float inputs, остальные SAN tracks, blend/restart в этой
render-связке, whole SMO resources, lit/custom material и живой GPU открыты.
Прежние actor-only suites отдельно покрывают более широкий playback набор.

```powershell
python research/native_workbench.py run pc-skin-san-render --deadline-utc 2026-09-07T16:00:00Z
```
