# `wxBossMovingState`

`wxBossMovingState` (Class ID `179B626C`) — потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxBossMovingState.cpp) сохраняет PC-ветви; для PS2 её область соответствия ограничена нормальными конечными float и знаковыми нулями. Переносимый объект не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `402D20` | `3F39A0` |
| Vtable | `6F8AF0` | `49A260` |
| Обновление, PC slot `30` | `5183F0` | `2C8670` |

Обновление получает объект motion по цепочке PC `owner+124 -> object+130`, PS2 `owner+130 -> object+13C`. Читает float `4`: при равенстве нулю применяет к ключу `FFFFFF8F` и не читает float `8`. Иначе читает float `8`; на PC при значении `> 0` задаёт `(key & FFFFFFCF) | 40`, в остальных случаях `(key & FFFFFFBF) | 30`. Затем применяет `F01FFFFF` и выполняет lookup. При различии handles освобождает старый, запускает новый с `mode=true`, `interrupt=true`, сохраняет после playback. При совпадении пропускает release/play. Control word не изменяется.

Оба знаковых нуля в первом поле выбирают неподвижную ветвь. PC unordered первого поля ведёт ко второму чтению; PC unordered второго поля выбирает mode `30`. Для нормальных конечных значений и нулей PS2 даёт те же ветви. Семантика NaN/Infinity и денормальных значений EE FPU пока не установлена и не подменяется поведением generic MIPS FPU.

Для двух чтений нужен обязательный [wxBossMovingStateHost](../../../Winx/Analysis/Host/wxBossMovingStateHost.h). Другие hooks, Reset и пустой Copy унаследованы. Clone создаёт начальное состояние; деструктор не останавливает playback. [Проверка](../../../Winx/Tests/wxBossMovingStateTests.cpp) включает порядок чтений и смену handle; полная игровая сцена остаётся внешней зависимостью.
