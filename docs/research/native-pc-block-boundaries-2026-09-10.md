# Уточнение exact boundaries в block tracer

После [первого измерения](native-pc-block-trace-2026-09-10.md) проверен ещё
один крайний случай стенда: запрошенная остановка или явный fixture находится
внутри translated block, а дальше лежит запрещённая инструкция. Первый
вариант проверял весь block заранее и отвергал даже ту часть, которую
выбранный prefix вообще не должен исполнять.

Синтетический пример `mov eax,1; hlt; ret`, boundary на `hlt`:
обычный tracer правильно останавливался с EAX1, новый возвращал отказ у
начала блока с EAX0. Аналогично преждевременно отвергался явный fixture,
который должен перехватить исполнение в этой точке. Это консервативный ложный
отказ исследовательского инструмента, не изменение оригинального поведения игры.

Теперь validation interval заканчивается перед ближайшим exact boundary
или fixture. Последующие bytes не исполняются и не входят в этот prefix.
Без остановки/fixture запрещённая инструкция по-прежнему приводит к отказу.
Набор seams на время вызова защищён readonly snapshot; между вызовами он
снова доступен для явной настройки.

Сохранён отрицательный regression до исправления:
`block-trace/boundary-regression-before.json`. После исправления прошли13
направленных tests за3,557 с, включая оба случая, frozen seams и прежние
cache/self-modification/cap проверки. Original CharacterStateMachine lifecycle
повторён только в изменённом block режиме: его semantic SHA снова совпал
с сохранённым instruction baseline. Остальные неизменные сравнения не повторялись.

Итог: `block-trace/boundary-correction-verification.json` и точные источники
в `block-trace/boundary-corrected-sources/`, относительно
`local-data/results/native-cycle-20260910-1900/`.
Ранние reports, benchmark source snapshots и Bloom run5 сохранены без замены;
исправление стенда не начисляет дополнительную изученность классов.
