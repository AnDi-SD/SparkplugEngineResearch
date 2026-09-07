# Нативные `spController` и `spSubController`

Дата PC-разведки: 2026-09-05; дополнено ночным manager checkpoint.
`spController` — substantial normal lifetime/copy slice; `spSubController`
остаётся abstract dependency. PS2 отложен.

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

Часовой PC-проход 2026-09-05 установил контракт дополнительного `+0x1C` slot:
actor вызывает его с float time, а `spNodeController` применяет полученный PRS.
Original method name остаётся неизвестным; классы не объявляются полностью
восстановленными. Сравнение с семислотовой vtable и
destructor `spAnimation` доказывает, что engine RTTI relation не совпадает с
физическим C++ inheritance layout. Это тот же тип различия, который уже
наблюдался у serializer-классов: type registry описывает engine compatibility,
а не обязательно буквальный исходный список C++ bases.

Проверка включена в `python research\inspect_animation.py`. Caller закреплён
отдельно в [PC runtime pipeline](native-pc-animation-runtime.md).
В исходники добавлен минимальный `spSubController` interface; это не byte-exact
C++ inheritance claim. Original slot/source names остаются неизвестными.

Продолжение [actor playback](native-pc-actor-playback.md) подтвердило physical
`spSubController` prefix и controller extent `0x1C`: enabled `+0x10`, next/prev
`+0x14/+0x18`. Protected constructor `0x00423010` ведёт к `0x004C4210` и
регистрирует controller в `spAnimationManager`; destructor снимает связь.
Original empty actor lifecycle исполняет обе операции. Следующий
[manager checkpoint](native-class-sp-animation-manager.md) исполнил настоящий
registry/frame, подтвердил copy `423100 -> 419AA0`: root copy, затем только
enabled byte `+0x10`. Links не копируются; actor clone получает новые runtime
defaults. Portable `spController` теперь регистрируется автоматически при наличии
manager и снимает регистрацию в destructor. Standalone mode без global и owner
guard — явные host safety deviations, не native fallback. Engine RTTI
`spAnimation` использует общий record, сохраняя отдельный physical named base.
Исходные пути controller TU остаются inferred; полный callback/reentry и startup
lifetime contract не объявляется закрытым.
