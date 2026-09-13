# spSphereBVSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spSerializer](../../../Sparkplug/Code/Sparkplug/spSerializer.h), [spSphereBV](../../../Sparkplug/Code/Sparkplug/spSphereBV.h), [spSphereBVSerializer](../../../Sparkplug/Code/Sparkplug/spSphereBVSerializer.h).

Статус: подтверждены identity, RTTI/lifetime, раздельный PC/PS2 ABI, два поля,
native epsilon подавления позиции и зеркальные target offsets reader-а.

## Поля

| ID | Имя | Запись | Mirror после чтения |
| ---: | --- | --- | ---: |
| 0 | `esfSphereBVPosition` | если любой компонент дальше `0.001` от нуля | `+0x18` |
| 1 | `esfSphereBVRadius` | всегда | `+0x24` |

Reader копирует прочитанную позицию и радиус одновременно в source и mirror
offsets. Writer сравнивает модуль каждого компонента позиции с float-константой
`0.001` (`0x3A83126F`); NaN вследствие ordered comparison не подавляется.

## Неизвестное

- оригинальные header/source paths и точные method names;
- полный `spSphereBV` inheritance/layout и смысл двух копий параметров;
- когда и кем синхронизируются source/mirror offsets после runtime mutation;
- допустимость отрицательного, NaN и бесконечного radius;
- контролируемый in-game mutation test.
