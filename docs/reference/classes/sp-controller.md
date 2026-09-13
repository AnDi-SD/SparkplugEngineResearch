# spController / spSubController

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spAnimation](../../../Sparkplug/Code/Sparkplug/spAnimation.h), [spBaseObject](../../../Sparkplug/Code/SparkBase/spBaseObject.h), [spController](../../../Sparkplug/Code/Sparkplug/spController.h), [spSubController](../../../Sparkplug/Code/Sparkplug/spSubController.h).

Engine RTTI задаёт цепочку `spAnimation -> spController -> spSubController ->
spBaseObject`:

| Класс | ID | Engine RTTI base | registration object |
| --- | ---: | --- | ---: |
| `spController` | `0x4FAD24F1` | `spSubController` | `0x0075DE50` |
| `spSubController` | `0x062C22ED` | `spBaseObject` | `0x00760340` |

Оба registration record не имеют factory. Обе vtable содержат восемь слотов
и заканчиваются одним abstract/pure body `0x0060DB76`. Registration getters —
`0x00423070` и `0x00467950`, deleting destructors — `0x004230E0` и
`0x00467970`. `spController` заменяет copy slot на защищённый thunk
`0x00423100`; `spSubController` оставляет base no-payload copy
`0x0040ECE0`. Clone у обоих остаётся null/base `0x004A1BF0`.
