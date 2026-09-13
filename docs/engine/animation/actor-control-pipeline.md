# PC actor: full-capacity Start и fade-stop до реального Tick

## Полный отказ Start

Два разных nonexclusive Start заполняют обе state и оба физических входа
каждого evaluator. Третий вызов **целиком** выполняет `5A1E30`: возвращает
`FFFFFFFF`, не меняет request weight0.25, playback bytes, inputs, queue/flush.
Ни `5A1C10` binder, ни `5FE9C0` insert не посещены. Опасная попытка третьего
input отсутствует; это доказанный ранний отказ при занятой capacity2.

При тех же занятых слотах restart уже используемой второй animation
возвращает1 и меняет request weight на1, сохраняя исходные restart semantics
state weight и real rebind. Source совпадает по всем выбранным state словам,
двум physical inputs с полными key caches и порядку events/flush.

## Deferred control → tick → Stop

После обычного Start команда `5A16D0(animation,.5,0)` задаёт rate2 и только
четыре state поля. Следующие manager frames выполняют настоящие actor,
evaluator, track sampling, NodeController и конечный `5A20A0` Stop:

| Шаг | Weight | Running | События/flush |
| --- | ---: | ---: | --- |
| helper | 1 | 1 | отсутствуют |
| tick0.25 | 0.5 | 1 | event8, flush |
| ещё0.25 | 0 | 1 | flush |
| ещё0.125 | 0 | 0 | event9, Stop flush, общий tick flush |

Stop suppressed: immediate event3 не отправляется. Оба controller inputs
очищают state/track, но сохраняют count/priority/cache. Binding count state
становится0, status3. При sample0 условие sample>threshold0 ложно, weight
не уменьшается. При fallback rate−0.5 tick0.25 увеличивает weight до1.125:
верхнего clamp в этой ветке нет. Эти варианты тоже сравнены.

## Границы описания

Сохраняются100000 instructions/2s/call,30s/child,64KiB arena и32KiB/request.
Общий third-input invariant для capacities19/40, динамический startup,
callback reentry и полная simultaneous animation/render frame остаются открыты.
