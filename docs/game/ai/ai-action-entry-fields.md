# AIAction: поля входа и общая выбранная цель

Методы и поля независимо прочитаны по оригинальным PC/PS2 инструкциям.
Все4000h bytes borrowed storage сравниваются,включая padding и незаписанные
области. Два fill A5/5A,два finite position набора,argument0/FFFFFFFF,
cached target0/12345678 и допустимые для выбранного метода null command
различают собственные изменения и сохранение входов. Указатели не выдаются
за штатно загруженный уровень. Адреса/offsets далее шестнадцатеричные.

## Общая цель

DarcyAttack,IcyAttack,StormyAttack,WinxFly вызывают wxCharacterRegistry из
global PC765AD8/PS2 49FD88. Registry factory4011D0/3F8360 и ID6EAA030D
установлены в исходном каталоге классов. Нужный helper —4E21D0/369B60.
PC cached target находится вregistry28,PS2 вregistry24. Ненулевое значение
возвращается сразу;нулевое при пустом первом массиве остаётся0.

| Действие | PC полный v10 | PS2 полный вход |
| --- | --- | --- |
| DarcyAttack | 5C23D0 | 248790 |
| IcyAttack | 5BE230 | 2591D0 |
| StormyAttack | 5C56F0 | 266580 |
| WinxFly | 5BC6A0 | 26D070 |

В суффиксе S0=action,V0=выход отдельного registry component. Граница после
собственных stores выбрана до epilogue. Более глубокий cold выбор первой
непустой группы через PS2 index helper29BD50 статически виден,но не
исполнялся этим пакетом. Cached target и пустой массив не доказывают весь
контракт registry. Registry не получает отдельного прироста оценки.

Все четыре действия записывают выбранную цель вaction3A8/PS2 3AC,включая0.
Остальные собственные изменения ниже указаны PC offsets;на PS2 они независимо
соответствуютoffset+4. Это не универсальное правило для любого поля игры.

| Действие | Обнуляемые words | Обнуляемые bytes | Прочие записи |
| --- | --- | --- | --- |
| DarcyAttack | 3B0,3B8,3C0,3B4,3CC,400 | 3AC,3F0,45C | word3BC=1,word3D0=2 |
| IcyAttack | 3B0,3B8,3C0,3B4,3C4,410 | 3AC,3AD,3E8,3E9,3EB | word3BC=1,word3C8=2,byte468=1 |
| StormyAttack | 3B0,3B8,3C4,3B4,3C8,3D4 | 3AC,3F4 | word3C0=1,word3D8=2,byte3D0=1 |
| WinxFly | 3AC,3B8,3B0,3B4 | 3BC | — |

## Полные простые методы

| Действие | PC / PS2 v10 | Результат |
| --- | --- | --- |
| Help | 5BAF70 /24FD50 | command word4=0;own byte3AC/3B0=0;word460/464=0 |
| GhoulScript | 5A9930 /24D410 | Сохранить node иcommand pointers,закрыть owner gate,сбросить два words,установить15000 |
| Wander | 5AB420 /26C1A0 | Обнулить два words и скопировать три finite position компонента |

Общая цепочка action20/24→owner144/154→character130/13C даётcommand.
Help требует ненулевойcommand. GhoulScript сохраняет такжеnull command:
own3B8/3BC=command;owner24→entity24 даётnode,сохраняемый вown3B0/3B4.
Owner byte14D/15D становится0,own words3A8/3AC и3AC/3B0 обнуляются,
word370/374=15000 decimal. Значение этого поля не переименовано в выдуманную
единицу времени.
