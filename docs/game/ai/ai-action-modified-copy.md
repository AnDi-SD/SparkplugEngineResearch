# AIAction: Copy изменённых полей

## Направление и общая база

В original API `this` — **источник**, единственный аргумент — **получатель**.
PC base `58FDD0` сначала вызывает `58FD30(destination)`, затем original
BaseObject Copy. PS2 `223250` содержит аналогичную inline очистку получателя.
При пустом current и контейнере получателя остальные байты общей полезной
нагрузки сохраняются; база не копирует её целиком из источника.

Все offsets далее шестнадцатеричные. Полный список независимо извлечённых
PC/PS2 полей сохранён в машиночитаемом контракте.

| Группа | Собственные переносы |
| --- | --- |
| AIAction, DroidWander, GhoulScript, Help, Hurt, IceWormHoles, Idle, KikoHole, KikoMoving, MikaelWandring, WinxFly | Нет: изменённая полезная нагрузка destination сохраняется |
| Attack | Четыре word PC3A8/3AC/3B0/3B4 |
| BacoAttack, GolemAttack | Пять word PC3A8..3B8 и byte3BC |
| DarcyAttack, IcyAttack | Word PC3A8/3B0/3B8/3BC; пропущенные промежутки сохраняются |
| IceGargoyleClaw/Withdrawl | Word PC3AC/3B0/3B4/3A8, в указанном порядке |
| IceGargoyleSleeping | Word PC3A8/3AC |
| FishWander, FlyingWander, Wander, IceGargoyleAttack | По12 переносов; наборы различаются |
| Frog, Knut, Minotaur, Stormy, Troll, Yeti | Отдельные подтверждённые наборы в контракте |

## Независимое PS2 подтверждение

PS2 собственные суффиксы начинаются после original base-call с V0=1. Регистры
source/destination/result восстановлены по конкретному прологу каждого Copy;
раскладка S0/S1/S2 различается между классами. Суффикс заканчивается до загрузки
сохранённого RA и LQ. Original completion `104F00` исполняется как настоящий
no-op; opaque ненулевой manager record — явный вход, не созданный PS2 manager.
