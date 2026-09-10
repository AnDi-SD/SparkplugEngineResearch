# Одна sort dependency для geometry и Alpha queue — предложение

**Статус: ожидает выбора.** Production policy, portable sorter и новые
original probes в рамках этого предложения не реализованы. Рекомендуется
одна явно выбранная общая policy `Msvcr71_7_10_7031_4`: перенос точного
алгоритма найденной CRT по CFG, с сохранением её permutations и comparator
calls. Default callbacks до решения остаются пустыми; DLL в tools не включать.

## Два исходных consumer, один импорт

| Consumer | Original call | IAT / dependency | Element / comparator |
|---|---|---|---|
| Geometry helper | `460CA9: FF15 60936D00` | `006D9360`, `MSVCR71.dll!qsort` | UInt16 ID, 2 bytes / `4607F0` |
| Alpha flush | `454864: FF15 60936D00` | тот же импорт | record 24 bytes / `454800` |

Оба общих caller уже имеют callback:
[GeometryHelper4604F0](../../Sparkplug/Analysis/PC/GeometryHelper4604F0.h) и
[spRenderer::AlphaDispatchForAnalysis](../../Sparkplug/Code/Sparkplug/spRenderer.h).
Новый bounded static anchor сохранён в
`local-data/results/tools-core-cycle-20260910-0730/occlusion-optimizer/sort-policy-alpha-import.json`.

Reference DLL: установленная `MSVCR71.dll` **7.10.7031.4**, 344064 bytes,
SHA256 `DCA0E5FAF6C94B6ADFF4D90D40795D5A91BA3A3059EA408E992A0F039A494D46`.
Export `7C382650`, shortsort `7C3825E0`, threshold count ≤ 8. Историческая DLL
из поставки игры не найдена. [Четыре actual geometry cases](tool-occlusion-geometry-helper-2026-09-10.md)
доказывают эту версию, а не универсальный игровой tie order. Original Alpha
flush прежде проверялся с явно объявленным no-op fixture; с этой DLL ещё нет
Alpha differential capture.

## Сохраняемая граница и предполагаемые файлы

Предлагаемые `Sparkplug/Analysis/PC/Msvcr71Qsort7107031.{h,cpp}` — один
versioned portable CRT helper, не новый игровой owner. Он переносит max-to-high
shortsort, median-of-three partition, выбор ветвей и swap order буквально;
не добавляет stable tie-break, pointer-ID порядок или нормализацию ключей.
Тонкие адаптеры существующих callbacks передают record access и исходный
comparator. Host 64-bit pointers не сужаются до original record24; проверяется
перестановка целых borrowed records, а не равенство размеров C++ ABI.

Geometry сохраняет 12-byte comparator, signed-zero различие, выбранного
representative и конечные IB/VB bytes. Alpha сохраняет normal/particle разделение,
unsigned priority и distance сравнения. Его comparator **никогда не возвращает
0**, даже для одной и той же записи, равных keys и NaN. Это различие tri-state
zero/tie contract; само по себе `cmp(a,a)==+1` **не** нарушает strict weak order
адаптера `cmp<0` на finite keys. Но такой адаптер и другой sort не сохраняют
автоматически observable tie permutations. `EnqueueAlphaForAnalysis` проверяет
pointers/capacity, а `BuildAlphaKey` не проверяет finite values; NaN потому
включён в предложенную boundary-проверку, без утверждения, что он встречается
в штатном игровом файле. Для одинакового priority NaN несравним с двумя
различными finite distances и нарушает transitive equivalence `cmp<0`.
Сохранить также sort перед flag44, вызов при пустой queue, borrowed lifetime,
adjacent support reuse и существующий live-count flush; не сортировать заново
добавленные callback-ом записи. Init и UI не входят в этот proposal.

Проверка — один новый `Sparkplug/Tests/spMsvcr71SortTests.cpp` плюс адресное
сравнение с настоящими DLL bytes: permutation, comparator call order, целые
records, guard bytes; для consumers — IB/VB либо prepare/render sequence.
Не более десяти small cases: **1)** Alpha count0; **2)** Alpha count1;
**3)** geometry count8 с duplicates; **4)** geometry count9 с тем же prefix;
**5)** geometry count12 all equal; **6)** captured duplicate5 representative4;
**7)** Alpha count8 с distinct priorities и particle records; **8)** Alpha
count9 с равными keys; **9)** Alpha count9 с повторными object pointers и
supports A/A/B; **10)** Alpha count9 с NaN и signed zeros. Original comparisons
получают прежние micro caps 100k/2 s, child30 s, один worker; stopped guest не
продолжается. Сейчас эти дополнительные cases только предложены.

Решение требуется именно об использовании именованной версии как общей host
policy: её точное восстановление снимает неизвестность алгоритма этой DLL,
но не доказывает историческую версию игры. До выбора обе существующие
callback-границы остаются явными, full Occlusion Init не объявляется готовым.
