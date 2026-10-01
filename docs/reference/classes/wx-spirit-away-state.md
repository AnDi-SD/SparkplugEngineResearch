# `wxSpiritAwayState`

`wxSpiritAwayState` (Class ID `F3333312`) — потомок `wxCharacterState` с selector `0`. [Общая реализация](../../../Winx/Code/wxSpiritAwayState.cpp) содержит собственную регистрацию, RTTI, фабрику и clone; все поведенческие slots используют общую базу. Исходный размер — `3C` на PC и PS2, собственных полей нет. Переносимый layout не является игровым ABI.

| Данные | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `403200` | `3F2CA0` |
| Vtable | `6F8F40` | `499E50` |

В обеих vtable все десять slots, соответствующие PC offsets `1C..40`, указывают ровно на функции [wxCharacterState](wx-character-state.md). Собственного алгоритма перемещения или ухода в этом классе нет. Вход освобождает pending handle и вызывает пустое обновление; выход освобождает handle. Проверка перехода `34` возвращает `true`, `38` — `false`; обработка события пустая. Control и ключ запроса эти hooks не меняют.

Copy унаследован от `spBaseObject` и пуст. Clone создаёт именно `wxSpiritAwayState`, регистрирует его и вызывает этот Copy: selector, flags и pending остаются значениями нового объекта. Деструктор не останавливает анимацию; reset очищает общие runtime-поля без stop/fade. Это подтверждённое наследование поведения, а не успешная заглушка по имени класса.

Освобождение живой анимации использует обязательный [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h). [C++ проверка](../../../Winx/Tests/wxGlyphSpiritStateTests.cpp) проверяет создание по Class ID, тип clone и унаследованные операции. Полная роль состояния в сцене определяется внешней state machine и её владельцем.
