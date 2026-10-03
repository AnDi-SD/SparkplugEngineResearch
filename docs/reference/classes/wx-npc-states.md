# `wxMikaelOpenGateState`, `wxMikaelWandringState`, `wxWandringNPCWaitState`

Три состояния используют базовые hooks [wxCharacterState](wx-character-state.md), selector `0` и исходный размер `44`. Дополнительные поля — byte `3C` и pointer `40`. Конструкторы PC и PS2 обнуляют byte `3C`, оставляя его padding и pointer `40` неинициализированными. Переносимая модель хранит неизвестный pointer отдельно. Она не претендует на игровой ABI.

| Класс | Class ID | Update PC / PS2 | Permission PC / PS2 |
| --- | --- | --- | --- |
| [wxMikaelOpenGateState](../../../Winx/Code/wxMikaelOpenGateState.cpp) | `539E4670` | `5A7EF0` / `3140A0` | constant true |
| [wxMikaelWandringState](../../../Winx/Code/wxMikaelWandringState.cpp) | `108B4B98` | `5A8080` / `314CE0` | `521970` / `314F40` |
| [wxWandringNPCWaitState](../../../Winx/Code/wxWandringNPCWaitState.cpp) | `4D9470B5` | `5A8430` / `318420` | constant true |

Общий PC entry `5A7DD0` читает `*(owner+124)+130`; соответствующие PS2 entry `314200`, `314EC0`, `318590` читают `*(owner+130)+13C`. Полученный pointer, включая null, сохраняется в `40`. Затем byte `3C` обнуляется, вызывается виртуальный update и базовый entry. Базовый entry освобождает текущий pending handle и вызывает update ещё раз. Кэш остаётся borrowed: восстановленные hooks не получают владение объектом.

OpenGate преобразует ключ в `(key & F1000000) | 01000000`, Wait — в `key & F0000000`. При совпадении результата lookup с pending update завершается без изменения byte `3C` или playback. При различии устанавливает byte `3C=1`, освобождает старый pending, запускает новый с `mode=false`, `interrupt=true` и сохраняет pending после playback.

Wandring преобразует ключ в `(key & F0000050) | 50`. Совпадение handle завершает update до completion query. При различии и ненулевом byte `3C` выполняет consuming query; byte получает логическое отрицание результата. Если byte после этого нулевой, старый pending освобождается, новый запускается с `mode=true`, `interrupt=true` и сохраняется после playback. Сохраняется исходная дополнительная ветвь: если после query подполе `(key & 70)` равно `40`, устанавливается byte `3C=1`, а playback использует `mode=false`. Обычная маска даёт `50`; изменение ключа внешним callback не предполагается автоматически.

Если byte остаётся ненулевым, pending не меняется, а control word PC `*(owner+12C)+4` / PS2 `*(owner+138)+4` получает bits `3DCCCCCD` (`0.1f`). Permission Wandring возвращает `byte3C==0` независимо от кода. Permission остальных двух классов возвращает true.

Exit, служебные hooks, пустой Copy и Reset унаследованы. Reset обнуляет базовые поля, но не меняет `3C/40`. Clone получает новые значения конструктора обеих платформ, без копирования живого кэша и byte исходного объекта. Деструктор не останавливает playback и не освобождает borrowed кэш.

[wxNPCStateHost](../../../Winx/Analysis/Host/wxNPCStateHost.h) подключает чтение внешнего объекта и запись control word. Общие lookup, predicate и animation controller используют обязательный базовый host. [Проверка](../../../Winx/Tests/wxNPCStateTests.cpp) охватывает entry, update, completion, clone/Copy и Reset. Исходные имена hooks и тип borrowed объекта неизвестны; полный игровой runtime остаётся открытой зависимостью. Аналитическая [структура общих операций](../../../Winx/Code/wxNPCStateOperationsForAnalysis.h) не объявляет дополнительную физическую базу C++.
