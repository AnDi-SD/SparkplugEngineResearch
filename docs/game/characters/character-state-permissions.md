# CharacterState: разрешения и режимы переходов PC/PS2

## Общий потребитель отметки

- Нулевой handle возвращает1 и сохраняет оба слова.
- Ненулевой handle, совпавший со вторым ненулевым словом, возвращает1,
  обнуляет второе и также первое, если оно совпадает.
- Иначе совпадение первого ненулевого слова возвращает1 и обнуляет первое.
- Если совпадений нет, возвращает0 и сохраняет оба слова.

## Группы разрешений v13

Все offsets ниже hex, значения target/current в таблице — десятичные.
`h`=state24, `current`=state10. Имена без префикса wx.

| Классы | Условие и эффект |
| --- | --- |
| Blast,DateReaction,SpiderAttack,Dispel,Hurt,OpenSecretPassage,WayToGo | h=0 →1, иначе Q |
| BasicMoving | target10/11 либо current≠9 либо h=0 →1; иначе Q |
| FishMoving | current=0 либо h=0 →1; иначе Q |
| FrogJumping,IceGargoyleAttack,KnutAttack | target10 либо h=0 →1; иначе Q |
| GhoulAttack | target9/10 либо h=0 →1; иначе Q |
| GolemAttack | target28 либо h=0 →1; иначе Q |
| TrixAttack | target10/17/28 либо h=0 →1; иначе Q |
| TrollAttack | target22/28 либо h=0 →1; иначе Q |
| GhoulJumping | target23 →0; иначе Q |
| Jumping | target3/4/5/23/31/41 →0; иначе Q |
| TrollMoving,YetiMoving | h≠0 и ненулевой byte3C/3D соответственно →Q; иначе1 |
| Vulnerable | current28 →raw byte3C; иначе h=0 →1, h≠0 →Q |
| YetiAttack | При current≠10 target10/33 разрешены сразу; остальные пути h=0 →1, иначе Q |
| IceWormAttack | packed character bits&7F80 равны0400/0500 →Q; иначе1 |
| GettingCollectible | Q и byte1D=0; Q вызывается первым |

Общие PC addresses не заменяют PS2 доказательство: отдельные PS2 тела
квалифицированы самостоятельными вызовами. Точные class/slot/address и входы
находятся в contract и selections.

| Классы | Собственная проверка |
| --- | --- |
| Attacking | target1 и low nibble packed character word=3 →0; иначе raw byte3C |
| FastAttack | То же исключение target1; иначе unsigned word3C≥word40 |
| ChargedAttack | Запрет target27/29/32 и target1 при low nibble=3 |
| Crouching | Запрет target1/3/4/5/8 |
| BloomDying,Missile | Возвращают raw byte3C, включая255 |
| DroidAttack | target10 либо byte3C≠0 →1; иначе0 |
| MikaelWandring,MinotaurMoving | byte3C=0 →1, иначе0 |
| LadderSlide | Разрешены только target0/10/14 |
| Ladder | target39 запрещён и обнуляет byte61 связанного move-record; остальные разрешены |
| SpiderHurt | При нулевом указателе по chain character→machine→otherCharacter→machine возвращает1; иначе raw byte4C |
| Dialogue,Reading | game-record word1B0 не равен70/71 соответственно |
| Falling | profile byte512 запрещает target3/4/5; target23 всегда запрещён; остальные требуют byte40≠0 либо target14/16/17 |
| PickFlower | target10 либо h=0 разрешены сразу; иначе Q. При разрешении обнуляет profile word504 |

## Режимы v14

| Классы | Результат |
| --- | --- |
| Attacking,FastAttack | 0 при target0/4/8/10/18, иначе1 |
| BloomDying,Dying,LadderSlide,Try2Hoist | 1 |
| Falling | target1 →1, иначе0 |
| GettingCollectible | target2 →1, иначе0 |
| PhysicalAttack | target9 →1, иначе0 |
| Jumping,GhoulJumping | target0/3 →2, иначе1 |
| Hurt | 0 при target0/19/20/21/22/23/24/29/30; также target4 при machine word PC14C/PS2158=4; остальные1 |

PC jump tables Attacking/Hurt и PS2 explicit compares сопоставлены по
перечисленным ветвям и соседним значениям, а не объявлены массивом bool.

## Границы описания

`MinotaurStunnedState`, PC521B20/PS2305BB0: при target11 сначала Q. Если он
вернул0, настоящий вызов PC512E40/PS22C8D60 получает this и argument0.
Остановились на входе этой зависимости; последующий return1 виден статически,
но полный путь не выполнен. Если первый Q вернул1, метод вызывает Q второй раз.
Для ненулевого совпавшего handle первый вызов потребляет отметки, второй
возвращает0. При h=0 оба вызова возвращают1. Это подтверждено PC/PS2 и не
«исправлялось» под ожидаемое поведение вьювера.

PC:400 полных возвратов и3 остановки на зависимостях. PS2:367 полных возвратов,
36 ограниченных исполнений. Для Falling,GettingCollectible,OpenGate,
MinotaurStunned исключены SQ-прологи и LQ-эпилоги; PickFlower входит в выбранного
по target преемника исходной ветви с SQ в delay slot. Все точные entry/stop,
входные регистры и исходные инструкции сохранены. Ни один успешный game callback
не подставлялся. Borrowed records не объявляются нормальной связкой всей игры.
