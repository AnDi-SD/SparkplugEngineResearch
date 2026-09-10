# CharacterState: разрешения и режимы переходов PC/PS2

Исследованы 68 привязок методов к 57 классам: 42 различных PC entry и 68 PS2
entry. После пилота выполнен общий пакет, затем 12 адресных проверок границ
общей зависимости: **806 успешных исполнений, по403 на платформу**. Сумма
времени внутри runner66,305s; внешние Python startups не включены. Независимые
части общего пакета запускались по два процесса. Прежние factory/Clone и
общие постоянные true/false методы не перепроверялись массово и не повышались.

Это развитие [базовых hooks](native-character-state-family-2026-09-10.md) и
[протокола машины](native-character-machine-protocol-2026-09-10.md).
`v13` проверяет разрешение, иногда с побочными действиями; `v14` возвращает
целочисленный режим **0/1/2**, а не произвольный bool. Официальные имена методов,
полей и перечисления режимов здесь не приписываются исходникам.

## Общий потребитель отметки

PC `004FB330`, PS2 `002A6C30`: входы receiver,handle,consume. Вызовы исследованных
состояний передают consume=1. Receiver хранится в state+18,handle — в state+24
на обеих платформах. Два сравниваемых слова receiver находятся PC170/174,
PS217C/180. Ниже `Q` означает настоящий вызов этого метода с consume=1:

- Нулевой handle возвращает1 и сохраняет оба слова.
- Ненулевой handle, совпавший со вторым ненулевым словом, возвращает1,
  обнуляет второе и также первое, если оно совпадает.
- Иначе совпадение первого ненулевого слова возвращает1 и обнуляет первое.
- Если совпадений нет, возвращает0 и сохраняет оба слова.

Проверены совпадение каждого слова отдельно, обоих сразу, отсутствие совпадения,
нулевые слова/handle и handles80000000/FFFFFFFF. Это идентификаторы для сравнения,
не разыменованные указатели. consume=0 сохраняет слова по прочитанному коду;
этот дополнительный режим здесь не исполнялся и отдельно не засчитывается.

Проверка разрешения может потребить отметку даже при итоговом запрете перехода.
`GettingCollectibleState` сначала вызывает Q, потом проверяет state+1D.
При ненулевом1D результат0, но совпавшая отметка уже удалена.

## Группы разрешений v13

Все offsets ниже hex, значения target/current в таблице — десятичные.
`h`=state24, `current`=state10. Имена без префикса wx.

| Классы | Условие и эффект |
|---|---|
| Blast,DateReaction,SpiderAttack,Dispel,Hurt,OpenSecretPassage,WayToGo | h=0 →1, иначе Q |
| DroidHurt,FlyingDodge,FrogAttack,FrogBackFlip,FrogHurt,Glyph,MosquitoAttack,MosquitoHurt,PhysicalAttack,PullLever,ShadowBeastHurt,ShadowBeastJumping,Try2Hoist | Q, включая нулевой h |
| BasicMoving | target10/11 либо current≠9 либо h=0 →1; иначе Q |
| FishMoving | current=0 либо h=0 →1; иначе Q |
| FrogJumping,IceGargoyleAttack,KnutAttack | target10 либо h=0 →1; иначе Q |
| GhoulAttack | target9/10 либо h=0 →1; иначе Q |
| GolemAttack | target28 либо h=0 →1; иначе Q |
| TrixAttack | target10/17/28 либо h=0 →1; иначе Q |
| TrollAttack | target22/28 либо h=0 →1; иначе Q |
| ShadowBeastAttack | target10 →1; иначе Q |
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
|---|---|
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

Packed character word PC140/PS214C. State character14, его move PC12C/PS2138;
machine PC124/PS2130. Spider chain после этого: machine138/144,
otherCharacter124/130. Game/profile — явно заданные ненулевые входные записи:
PC globals755294/765AD4, PS2 GP−4424/−44F4 при GP4A4170. Их lazy factories и
нормальный startup не выполнялись и не считаются закрытыми.

Falling PC содержит signed comparisons, PS2 — equality cases. Проверенные
raw target inputs, включая80000000/FFFFFFFF, дали одинаковый результат.
PS2 packed bits переносит LWC1/SWC1; здесь нет float arithmetic и не заявляется
общая эквивалентность FPU. Не проводится нормализация raw flag255 в1 там, где
оригинал возвращает byte напрямую.

## Режимы v14

| Классы | Результат |
|---|---|
| Attacking,FastAttack | 0 при target0/4/8/10/18, иначе1 |
| BloomDying,Dying,LadderSlide,Try2Hoist | 1 |
| Falling | target1 →1, иначе0 |
| GettingCollectible | target2 →1, иначе0 |
| PhysicalAttack | target9 →1, иначе0 |
| Jumping,GhoulJumping | target0/3 →2, иначе1 |
| Hurt | 0 при target0/19/20/21/22/23/24/29/30; также target4 при machine word PC14C/PS2158=4; остальные1 |

PC jump tables Attacking/Hurt и PS2 explicit compares сопоставлены по
перечисленным ветвям и соседним значениям, а не объявлены массивом bool.

## Две зависимые ветви и границы исполнения

`MinotaurStunnedState`, PC521B20/PS2305BB0: при target11 сначала Q. Если он
вернул0, настоящий вызов PC512E40/PS22C8D60 получает this и argument0.
Остановились на входе этой зависимости; последующий return1 виден статически,
но полный путь не выполнен. Если первый Q вернул1, метод вызывает Q второй раз.
Для ненулевого совпавшего handle первый вызов потребляет отметки, второй
возвращает0. При h=0 оба вызова возвращают1. Это подтверждено PC/PS2 и не
«исправлялось» под ожидаемое поведение вьювера.

`OpenGateState`, PC5A77C0/PS22D04E0: Q=0 возвращает0. Q=1 передаёт this и
arguments27DE,12,0,0 в настоящий общий message dispatcher PC40EC00/PS21007A0.
Проба заканчивается на входе dispatcher. Получатели сообщения, изменения из
callbacks и полный успешный путь остаются открыты; продолжение требует
отдельного исследования subscription manager.

PC:400 полных возвратов и3 остановки на зависимостях. PS2:367 полных возвратов,
36 ограниченных исполнений. Для Falling,GettingCollectible,OpenGate,
MinotaurStunned исключены SQ-прологи и LQ-эпилоги; PickFlower входит в выбранного
по target преемника исходной ветви с SQ в delay slot. Все точные entry/stop,
входные регистры и исходные инструкции сохранены. Ни один успешный game callback
не подставлялся. Borrowed records не объявляются нормальной связкой всей игры.

Все8 записи сравнивались целиком, включая сохраняемые bytes. Новых native
отказов нет. Первоначальная подготовка пыталась поместить130 PS2 windows в
capture с лимитом128: сохранён отказ подготовки, PS2 разнесён на два пакета;
прежний PC capture не перезаписывался. Это не ошибка исполняемого файла.

Evidence: `local-data/results/native-cycle-20260911-0730/character-state-permissions/`.
Учёт повышает57 уже зарегистрированных классов по объёму новых методов.
Полностью восстановленных классов и новых C++ реализаций этот блок не добавляет.
