# `wxShadowBeastJumpingState`

`wxShadowBeastJumpingState` (Class ID `303A77C2`) — потомок `wxCharacterState` с selector `1`. [Общая реализация](../../../Winx/Code/wxShadowBeastJumpingState.cpp) сохраняет собственные hooks PC/PS2. Исходный размер — `3C` на обеих платформах; дополнительных полей нет. Переносимый класс не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `4041C0` | `3F02A0` |
| Vtable | `6F9FD8` | `499130` |
| Вход, PC slot `1C` | `523800` | `30FFA0` |
| Выход, PC slot `20` | `5237B0` | `30FF40` |
| Обновление, PC slot `30` | `523790` | `30FF80` |
| Проверка перехода, PC slot `34` | `523770` | `310120` |

Вход освобождает старый pending handle ещё до изменения ключа. Затем вычисляет `(key & F0087FA2) | 00080022`, выполняет lookup и запускает анимацию с `mode=false`, `interrupt=true`. После playback сохраняет handle, очищает once-флаг `1C` и вызывает виртуальное обновление `30`. Результат — `true`.

Обновление только задаёт `(key & FFFFFFF2) | 2`. Выход сначала выполняет такую же запись, затем обнуляет control word `4`, очищает младшие четыре бита ключа и вызывает общий выход [wxCharacterState](wx-character-state.md), освобождающий handle. Промежуточная запись mode `2` предшествует control callback и сохранена в реализации.

Переход `34` всегда вызывает completion consumer с текущим handle и `consume=true`, включая случай пустого handle. Target не используется. Copy унаследован от `spBaseObject` и пуст. Clone получает selector `1`, начальные once-флаги и пустые привязки; деструктор не останавливает анимацию. Остальные hooks используют общую базу; собственные PS2 leaves для `38/3C` эквивалентны её `false`/пустой операции.

Внешние анимационные объекты и control подключаются через обязательный [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [C++ проверка](../../../Winx/Tests/wxShadowBeastJumpingStateTests.cpp) охватывает сообщения callbacks, промежуточные ключи, освобождение, clone и потребление completion-записей. Полная игровая сцена остаётся внешней зависимостью.
