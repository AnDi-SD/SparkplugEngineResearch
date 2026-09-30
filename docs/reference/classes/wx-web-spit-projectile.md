# `wxWebSpitProjectile`

`wxWebSpitProjectile` (Class ID `5CE65EBF`) зарегистрирован как потомок [`wxProjectile`](wx-projectile.md). PC и PS2 размер — `100`; собственный хвост `EC..FF` содержит байт `EC`, слово `F0`, две ссылки `F4/F8` и слово `FC`. Переносимая [реализация](../../../Winx/Code/wxWebSpitProjectile.cpp) использует общую C++ базу, а [PC](../../../Winx/Analysis/PC/wxWebSpitProjectileAbi.h) и [PS2](../../../Winx/Analysis/PS2/wxWebSpitProjectileAbi.h) layout оставлены отдельно.

| Контракт | PC | PS2 |
| --- | ---: | ---: |
| Factory | `401A00` | `3F6CA0` |
| Constructor | factory path | `2F3B70` |
| Vtable | `6F6C28` | `49B1F0` |
| Copy | `501640` | `2F2F50` |

PS2 конструктор вызывает `wxProjectile`, обнуляет байт `EC` и слова `F0/F4/F8/FC`. PC factory snapshot показывает те же определённые значения. Copy обеих платформ напрямую вызывает объектный Copy, переносит **только байт** `EC`, слово `F0`, две ссылки `F4/F8` с 16-битным освобождением и удержанием, затем слово `FC`. Промежутки `ED..EF` сохраняются. Общий payload `wxProjectile` этим Copy не копируется, новый Actor не создаётся. Внешние объекты ссылок проходят через `wxProjectileHost`.

PC деструктор освобождает ссылки `F4`, затем `F8`, и передаёт управление деструктору `wxProjectile`. [WinxWebSpitProjectileTests](../../../Winx/Tests/wxWebSpitProjectileTests.cpp) проверяет эти границы, clone и порядок операций. Исходные измерения factory/RTTI/пустого clone и удаления находятся в `local-data/results/native-cycle-20260910-1900/projectile/`; собственный Copy и конструктор сопоставлены по оригинальным PC/PS2 инструкциям. Игровой запуск паутины, столкновения и полный PS2 teardown ещё не восстановлены.
