# `wxSpiderMovingState`

`wxSpiderMovingState` (Class ID `463733DF`) — потомок [wxCharacterState](wx-character-state.md), selector `0`, исходный размер `3C`, дополнительных полей нет. [Переносимая реализация](../../../Winx/Code/wxSpiderMovingState.cpp) не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `4039E0` | `3F17A0` |
| Vtable | `6F9880` | `4997C0` |
| Обновление, PC slot `30` | `520930` | `2F2350` |

Обновление читает motion из control word `4`. Если motion `< 0.1f`, вызывает setter скорости consumer со значением `1.0f`, применяет к ключу `FFFFFF8F` и обнуляет control word `4`. Иначе передаёт setter исходный motion и задаёт `(key & FFFFFFDF) | 50`. Затем применяет `F007FFFF`, выполняет lookup. Только при различии handle освобождает старый, запускает новый с `mode=true`, `interrupt=true` и сохраняет после playback. Одинаковый handle не отменяет изменение скорости, ключа и control word.

Равенство порогу и PC unordered выбирают движение. Для PS2 подтверждены нормальные конечные float и знаковые нули; специальные и денормальные значения остаются отдельной границей переноса. Helper скорости PC `512F00` / PS2 `2C8CB0` обращается к consumer PC `4FB6B0` / PS2 `2A6C20`. PS2 setter записывает float в `consumer->field134->field20`. В PC setter используется runtime-переходник: его внутренний эффект нельзя подменять предполагаемым layout.

Требуются обязательные [motion](../../../Winx/Analysis/Host/wxCharacterMotionStateHost.h) и [speed](../../../Winx/Analysis/Host/wxCharacterSpeedStateHost.h) интерфейсы одного host. Остальные hooks, Reset и пустой Copy унаследованы; clone создаётся с начальными flags и пустыми привязками. [Проверка](../../../Winx/Tests/wxAdditionalCharacterStateTests.cpp) описывает границу компонентного выполнения, включая совпадающий handle и порог.
