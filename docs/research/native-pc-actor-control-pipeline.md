# PC actor: full-capacity Start и fade-stop до реального Tick (CP68)

2026-09-07, pristine PC executable SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [CP67 controls](native-pc-actor-controls.md); та же исходная
`bbush.san`, три independently owned animations, два named Node и настоящие
controller/evaluator/name registry. Source file/stream boundaries прежние.

Factory действительно читает объявленное global capacity2 и выделяет192
bytes под две state. Это проверка конкретной конфигурации, не исполнение
всего игрового init и не универсальный предел всех actor.

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
|---|---:|---:|---|
| helper | 1 | 1 | отсутствуют |
| tick0.25 | 0.5 | 1 | event8, flush |
| ещё0.25 | 0 | 1 | flush |
| ещё0.125 | 0 | 0 | event9, Stop flush, общий tick flush |

Stop suppressed: immediate event3 не отправляется. Оба controller inputs
очищают state/track, но сохраняют count/priority/cache. Binding count state
становится0, status3. При sample0 условие sample>threshold0 ложно, weight
не уменьшается. При fallback rate−0.5 tick0.25 увеличивает weight до1.125:
верхнего clamp в этой ветке нет. Эти варианты тоже сравнены.

## Проверки и границы

`pc-actor-control-pipeline`: **5 exact captures,30 native assertions**.
Максимум4841 instructions в рабочей фазе;15760 bytes arena; все allocations
actor, SAN, controllers, registry/name owners освобождены. Это дополнительные
целые последовательности, не повторный подсчёт прежних actor suites.

Native manager действительно dispatch-ит Tick. Source observation harness
использует уже установленную схему `pcActorScenario`: временно gate actor
для продвижения manager counter, затем вызывает тот же Tick явно, чтобы
получить actions. Настоящий source manager→actor dispatch отдельно проверен
в CP66 и C++ integration suite. Такая граница не скрыта как whole source
event dispatcher. Node float transforms в этом capture не заявлены как
побитово сравненные: проверяются state raw words, input caches и actions;
read→SAN→Skin PRS/constants находятся в отдельном CP66.

Сохраняются100000 instructions/2s/call,30s/child,64KiB arena и32KiB/request.
Общий third-input invariant для capacities19/40, динамический startup,
callback reentry и полная simultaneous animation/render frame остаются открыты.

```powershell
python research/native_workbench.py run pc-actor-control-pipeline --deadline-utc 2026-09-07T16:00:00Z
```
