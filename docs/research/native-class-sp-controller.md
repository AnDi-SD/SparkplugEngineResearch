# Нативные `spController` и `spSubController`

Дата PC-разведки: 2026-09-05. Статус: dependency scout для `spAnimation`;
PS2 отложен.

Engine RTTI задаёт цепочку `spAnimation -> spController -> spSubController ->
spBaseObject`:

| Класс | ID | Engine RTTI base | registration object | PC vtable |
|---|---:|---|---:|---:|
| `spController` | `0x4FAD24F1` | `spSubController` | `0x0075DE50` | `0x006DC86C` |
| `spSubController` | `0x062C22ED` | `spBaseObject` | `0x00760340` | `0x006E83E4` |

Оба registration record не имеют factory. Обе vtable содержат восемь слотов
и заканчиваются одним abstract/pure body `0x0060DB76`. Registration getters —
`0x00423070` и `0x00467950`, deleting destructors — `0x004230E0` и
`0x00467970`. `spController` заменяет copy slot на защищённый thunk
`0x00423100`; `spSubController` оставляет base no-payload copy
`0x0040ECE0`. Clone у обоих остаётся null/base `0x004A1BF0`.

Этого недостаточно, чтобы назвать дополнительный virtual slot или объявить
классы полностью восстановленными. Однако сравнение с семислотовой vtable и
destructor `spAnimation` доказывает, что engine RTTI relation не совпадает с
физическим C++ inheritance layout. Это тот же тип различия, который уже
наблюдался у serializer-классов: type registry описывает engine compatibility,
а не обязательно буквальный исходный список C++ bases.

Проверка включена в `python research\inspect_animation.py`. Следующий проход
должен найти callers abstract slot, раскрыть protected copy `0x00423100` и
установить, есть ли у controller-классов собственные поля либо только
interface/registry роль.
