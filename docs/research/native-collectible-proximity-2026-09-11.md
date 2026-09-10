# CollectibleGem: flags и проверка близости

45 входных сценариев дали102 успешных исполнения с первого раза.
PC:21 полный геометрический helper и24 полных wxCollectibleGem virtual11.
PS2:33 geometry prefixes,12 собственных gate components и12 отдельных
comparison tails. Пилот5 исполнений за0,694s,общий пакет97 за6,741s внутри
процессов. Это проверка predicate,не полный сценарий подбора/начисления предмета.

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

21 пара helper inputs:force0/1/FF,высоты±149/±150,tolerance150;дополнительно
height0/1,tolerance0/−1. PC полный x87 результат считан без изменения guest,
все node/record bytes сохранены. Проверены конечные точно представимые значения;
FPU exceptions,NaN/Inf/denormal и все режимы округления не квалифицированы.

PS2 исполняет реальный scalar fabs `00423B58` и vector constructor
`00109AF0`. Prefix до `00293280` проверяет получившиеся три компонента в
stack+30 и все32KiB внешних записей. R5900 accumulator начинается с
`4602101A` в293280;накопление не исполнялось и не подменялось формулой.
PS2 возвращённый square не заявляется измеренным результатом этого prefix.

## wxCollectibleGem virtual11

PC `00544C90`,PS2 `003A2E40`,vtable `006FCF20 /004976A0`.
Сам вызов не устанавливает полный смысл всех полей,поэтому они обозначены
смещениями и проверенным действием.

1. При byte149 PC /155 PS2 nonzero возвращает false сразу,не вызывая v10.
2. Иначе PC через настоящую vtable вызывает inherited wxEntity v10
   `004DA930`;PS2 цель slot также `00286460`.
3. Читает flags через object24→wordC. Если byte148/154 равен0 и flags bit2
   установлен,вызывает описанный helper с node из `[object144/150]+24`
   и node из `[object24]+24`,force0,tolerance150.
4. Если его square<=radius²,где radius из140/14C,возвращает true.
   При неуспехе этого условия или отсутствии geometry branch возвращает
   `byte148/154!=0 || (flags & 1A)==0`.

Во всех PC случаях использованы настоящие vtable/v10/helper/getter timer,
borrowed пустой cached список,byte28=1 и distance sentinel7F7FFFFF в3C.
Свежий v10 записывает flags `FFFFFF00 | (enabled?2:0) | (pause?8:0)`.
Результат caller проверен вместе с этим изменением recordC;вся остальная
область32KiB сохраняется. У выключенного Gem149/155 recordC не меняется.

24 Gem inputs:ранний byte1/FF;byte148/154=0/1/FF;enabled0/1,pause0/FF;
высоты±149/±150 и radii0/4/5/6/−5/150/151. Равенство square=radius² проходит;
отрицательный radius−5 в локальном сравнении даёт тот же квадрат,что5.
Это диагностические inputs,не доказательство их достижимости в обычной игре.

PS2 ранний gate исполняется от3A2E54 до3A2EF4,после SQ prologue.
Остальные caller components начинают с3A2E78 после v10 с явно заданными
cached flags по ранее доказанному
[wxEntity контракту](native-entity-reference-flags-2026-09-10.md).
Этот v10 не вызывается и не подменяется успешной заглушкой;это граница
отдельного компонента. Доступные gate ветви доходят до epilogue3A2EF4.
Geometry ветви исполняют настоящий helper до accumulator293280.

12 независимых fresh tails от3A2EB4 получают **объявленный** finite square
вF0,radius вF20,flags вS0 иGem вS1,после чего исполняют оригинальные
radius²,comparison иfallback. Они подтверждают заключительную логику,но
не превращают несколько компонентов в полную PS2 Gem транзакцию.

## Границы и учёт

PC micro100k/2s,arena64KiB;PS2 scalar5000 instructions/500ms,32KiB records
и stack page;внешний process30s. Все guests независимы. Создание/привязка
nodes,Gem lifecycle,сцена,cache population,collection events иUI не изучались
этой пробой. Неподдержанная PS2 accumulation остаётся явно открытой.

Два updates:Gem PC20→25,PS215→20. Helper не зарегистрирован отдельным
классом и не получает выдуманного class score;общий wxEntity повторно не
повышается. Новых C++ и полностью закрытых классов нет.
[Контракт](../../research/collectible-proximity-contracts-2026-09-11.json).
Локальные captures/results в `local-data/results/native-cycle-20260911-0730/collectible-proximity/`
не входят в Git.
