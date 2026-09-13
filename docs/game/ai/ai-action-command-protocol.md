# AIAction: завершение команд, входные параметры и выбор состояния

## Команды и завершение действия

Исполняются настоящие virtual v11, выбранные по ранее доказанным таблицам каждого класса. Borrowed вход содержит отдельные записи action, owner, character, command, perception, move. Проверяются все байты шести записей.

Общая цепочка к command: PC `[action+20] → [owner+144] → [character+130]`;
PS2 `24 → 154 → 13C`. Сбросы не проверяют эти указатели на null.
Все числа смещений и адресов ниже шестнадцатеричные.

| Действия (префикс wx, суффикс AIAction) | Обнуляемые command bytes | Собственный zero word PC/PS2 |
| --- | --- | --- |
| Attack | 1D,20,21,1F | 3A8/3AC |
| BacoAttack, IceWormAttack | 1D,20 | 3A8/3AC |
| DarcyAttack, TrollAttack | 1D,20,21,22,23,24 | 3A8/3AC |
| DroidAttack, SpiderAttack | 1D,20,21 | 3A8/3AC |
| DroidWander | 1C | нет |
| FrogAttack | 1D,20 | 3A8/3AC |
| Hurt | 5C | нет |
| IceGargoyleAttack | 20,21 | 3D8/3DC |
| IceGargoyleClaw, IceGargoyleWithdrawl | 20,21 | 3AC/3B0 |
| IceGargoyleSleeping | 1B | нет |
| KnutAttack | 1D,5C,20,21,22,23,24 | 3A8/3AC |
| MinotaurAttack | 1D,20 | 3A8/3AC |
| MinotaurDefend | 1D,1F | 3A8/3AC |
| MosquitoAttack | 1D,20 | 3A8/3AC |
| TrollBeforeFight | 1D,20,21,22,23,24 | нет |

Frog и оба Minotaur берут command прямо из собственного поля: соответственно
PC/PS2 `3E8/3EC`, `3C0/3C4`, `3B4/3B8`. Общая цепочка у них не используется.
У DroidAttack, SpiderAttack и ShadowBeast дополнительно устанавливаются
perception `byte70=1`, `word74=41C80000` (25.0f), `byte78=0`.
PC выполняет original helper `515350(character,0)` и читает `character+154`;
PS2 читает `character+160` прямо. Helper не подменялся.
Mosquito дополнительно обнуляет move `1E0/1EC`; ссылка move находится
в character `12C/138`. Назначения отдельных command bits пока не переименованы
в предположительные игровые enum.

## Входные методы v10

FlyingWander обнуляет собственные PC `3B8/3BC`, PS2 `3BC/3C0`.
IceGargoyleSleeping обнуляет PC `3A8/3AC`, PS2 `3AC/3B0`.
TrollBeforeFight обнуляет четыре последовательных word с PC `3A8`, PS2 `3AC`.
Входной параметр 0 или FFFFFFFF не меняет эти эффекты.

## Выбор обработчика v7

Исполнение останавливается **на входе настоящего обработчика**, до выполнения его тела. Неизменность всего action и правильный this проверяются. Это доказательство выбора ветви, не выполнения атаки, таймеров или перехода внутри выбранного обработчика.

| Действие | State PC/PS2 | Выбор source-level virtual slot на обеих платформах |
| --- | --- | --- |
| IceGargoyleSleeping | 3A8/3AC | 0→18, 1→19, остальные возвращают true без вызова |
| TrollBeforeFight | 3A8/3AC | 0→17, 1→18, 2→19, 3→20, остальные→17 |
| TrollAttack | 3AC/3B0 | 0→18,1→19,2→20,3→21,4→22,5→23,6→28,7→24,8→25,9→26,10→27; остальные→18 |
