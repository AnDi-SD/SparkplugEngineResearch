# PC Skin: сохранённый граф read → render (CP65)

2026-09-07. Исходный WinxClub.exe SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

После целиком выполненных FAT `466B90` и Skin reader `491170` тот же Skin,
его Node и обе выделенные таблицы используются полным `46A240` до загрузки
shader constants и indexed draw. Common resolver `4678B0` и Node reader
`463A70` выполняются оригинальными инструкциями. Input содержит position
`{1,2,3}`, scale `{2,3,4}` и inverse-bind translation `{5,6,7}`. Палитра
получает translation `{11,20,31}`; сравниваются все её байты и device events.

Три сценария: обычный успех, false post callback и weights=0. Source harness
читает ровно те же directory/payload bytes через настоящий FAT, Node и Skin
сериализаторы. После уничтожения read context сохранённый Skin передаётся
в восстановленный render caller. После [CP107](native-pc-whole-skin-scene.md)
source harness явно сохраняет владельцев из context до конца render, а Skin
заимствует кости через weak_ptr. Native Skin заимствует pointer, стенд также
сохраняет Node до конца render.

## Ускорение без расширения лимитов

Старый линейный стендовый аллокатор никогда не использовал освобождённые
диапазоны. Reference resolver даже на успешном чтении создаёт настоящий
диагностический singleton размером `1024h`; вместе с renderer backing
storage `F2F8h` это мешало соединить две завершённые фазы в 64 KiB.

`pc_reusing_arena_fixtures.py` задаёт новый **внешний allocator contract**
в пределах той же arena. Большие engine blocks размещаются с её верхнего
края, остальные — снизу; освобождённые соседние диапазоны объединяются.
Это не реконструкция адресной политики исходного allocator. Каждый engine
allocation получает generation, которая должна быть освобождена ровно
один раз. Живые диапазоны не перемещаются и не пересекаются. Raw caller
inputs остаются зарезервированными до завершения fixture.

Между завершёнными API вызовами исполняются настоящие **очистки записей**
FAT `466760` и serializer manager `4228A0`, deleting destructor Skin serializer,
затем `4C3430` диагностического
singleton; проверяется обнуление `755264`. После подготовки renderer
адреса и все байты Skin/Node/arrays должны оставаться неизменными. Это
явная межфазная teardown-последовательность стенда; raw FAT/manager backing
inputs остаются owned стендом. Раннее описание ошибочно называло обе
entry-cleanup функции деструкторами; это уточнено при следующем статическом
разборе. Полный игровой caller
с таким управлением singleton не заявляется.

Лимиты прежние: 100000 инструкций / 2 s на call, 30 s на child, 64 KiB
arena, 32 KiB на engine allocation request. Нет map-on-fault, перемещения
живых объектов, повторного проигрывания конструктора или продолжения
прерванного лимитом вызова. Пиковая занятость 64944 bytes, 26 повторных
использований диапазона, 35 engine allocation generations; все освобождены.
Рабочее чтение — 50702 инструкции, render — до 15288.

`pc-skin-loaded-render`: 3 exact captures, 57 native assertions
(по 10 linked-phase и 9 render assertions). Source проверяет также полное
чтение и canonical Node reference. Предыдущие prepared-graph probes сохраняют
свой обычный аллокатор и прежнюю семантику.

## Остаток

Mesh/device buffers, fallback material и shader cache по-прежнему явно
подготовлены на границе потребителя, как в [CP64](native-pc-skin-render.md).
Здесь прочитаны Skin и его bone graph, а не полный SMO со всеми ресурсами.
SAN actor → тот же Node → palette, full frame, lit/custom material,
compiler miss из geometry и живой GPU остаются открыты.

```powershell
python research/native_workbench.py run pc-skin-loaded-render --deadline-utc 2026-09-07T16:00:00Z
```
