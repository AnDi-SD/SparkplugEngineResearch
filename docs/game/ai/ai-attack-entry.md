# AIAction: начальные поля девяти атак

82 успешных исполнения PC/PS2 проверяют virtual v10 девяти классов.
По25 полных возвратов и16 границ настоящего owner selector на платформу.
Начальные поля, оригинальный поиск цели и выбранная ветвь подтверждены;
выполнение всех последующих атак и штатный startup не заявляются.

Ниже смещения и адреса шестнадцатеричные. Указатель owner: action20/24;
character: owner144/154; command: character130/13C; perception:
character154/160. Входной граф заимствован явно и целиком покрыт24KiB guard.

При отсутствии цели первые шесть всё равно сначала записывают начальные
поля, затем запрашивают owner action key1, кроме IceWorm, который запрашивает0.
Камерные три класса также запрашивают0. Parameter всегда0 на этих путях.
Выполнение останавливается на настоящем `591C80/225DF0`, выбранном
через original base AIBehavior vtable. Возврат selector не подменяется.

## Начальные поля

В таблице указаны PC offsets; для перечисленных собственных полей action
PS2 offset больше на4. Command offsets одинаковы.

| Класс | Эффекты входа |
| --- | --- |
| BacoAttack | command1D=1;target3A8;state3AC=4;word3B4=47AFC800(90000f);word3B0=0;byte3BC=0;word354=0 |
| DroidAttack | command1D=1,1C/20/21=0;target3A8;state3AC=0;word3B4=47742400(62500f);word3B0=0;byte3BC=0 |
| FrogAttack | command1D=1,command pointer3E8;target3A8;state3AC=0;word3B0=0;byte3BC=0;float3B4=160000×параметр character config |
| IceWormAttack | command1D=1;target3A8;state3AC=0;word3B0=0;условное копирование трёх координат в3B8/3BC/3C0 |
| IceGargoyleAttack | Node3D8;state3DC=2;word3E0=0;byte3B4=0 |
| IceGargoyleClaw, IceGargoyleWithdrawl | Node3AC;state3B0=1;word3B4=0 |

Droid/Shadow/Spider при найденной цели дополнительно записывают perception:
byte70=0,word74=7F7FFFFF,byte78=1. При отсутствующей цели эти поля сохраняются.
PC оба раза получает существующий perception через оригинальный515350,
PS2 читает поле character непосредственно.

В успешных результатах60 original perception calls,4 registry calls,
20 distance calls; каждый отдельно наблюдался. PS2 интерпретировал370
SQ/LQ и93 ACC операции в ранее квалифицированных узких профилях;
все адреса/значения сохранены, исходные code bytes неизменны.
Численные входы конечны и ограничены; general EE FPU, нечисловые значения,
произвольные масштабы и нормальная подготовка всех записей не заявляются.

18 platform updates по5 за девять новых v10 контрактов. Старый поиск цели,
distance helper и v12 повторно не засчитываются. Нового C++, полностью
закрытых классов или изменения игровых алгоритмов нет.
