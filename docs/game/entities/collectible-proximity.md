# CollectibleGem: flags и проверка близости

## Геометрический helper

PC `00597660` читает позиции двух nodes из74/78/7C и вызывает
`00597600` с координатами по значению, byte force и float tolerance.
PS2 `00293210` читает70/74/78,принимает force вA2 и tolerance вF12.
При nonzero force либо **abs(second.y-first.y)<tolerance** обе локальные
Y становятся0. После этого вычисляется квадрат расстояния. Сами nodes
не меняются. При равенстве tolerance вертикаль сохраняется.

Вызов из Gem использует force0,tolerance150. Поэтому3/149/4 даёт square25,
а3/150/4 —22525. Знак высоты не меняет правило. Это не сферическое расстояние
при любых входах и не длина с square root.

## wxCollectibleGem virtual11

1. При byte149 PC /155 PS2 nonzero возвращает false сразу,не вызывая v10.
2. Иначе PC через настоящую vtable вызывает inherited wxEntity v10
   `004DA930`;PS2 цель slot также `00286460`.
3. Читает flags через object24→wordC. Если byte148/154 равен0 и flags bit2
   установлен,вызывает описанный helper с node из `[object144/150]+24`
   и node из `[object24]+24`,force0,tolerance150.
4. Если его square<=radius²,где radius из140/14C,возвращает true.
   При неуспехе этого условия или отсутствии geometry branch возвращает
   `byte148/154!=0 || (flags & 1A)==0`.

24 Gem inputs:ранний byte1/FF;byte148/154=0/1/FF;enabled0/1,pause0/FF;
высоты±149/±150 и radii0/4/5/6/−5/150/151. Равенство square=radius² проходит;
отрицательный radius−5 в локальном сравнении даёт тот же квадрат,что5.
Это диагностические inputs,не доказательство их достижимости в обычной игре.

12 независимых fresh tails от3A2EB4 получают **объявленный** finite square
вF0,radius вF20,flags вS0 иGem вS1,после чего исполняют оригинальные
radius²,comparison иfallback. Они подтверждают заключительную логику,но
не превращают несколько компонентов в полную PS2 Gem транзакцию.
