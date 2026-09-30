# `wxStingerProjectile`

`wxStingerProjectile` (Class ID `206A45A2`) наследует [`wxProjectile`](wx-projectile.md). На PC и PS2 объект имеет размер `F8`: общий префикс `EC`, две ссылки в `EC/F0` и слово в `F4`. [Переносимый класс](../../../Winx/Code/wxStingerProjectile.cpp) физически использует общую C++ базу `wxProjectile`; [PC](../../../Winx/Analysis/PC/wxStingerProjectileAbi.h) и [PS2](../../../Winx/Analysis/PS2/wxStingerProjectileAbi.h) ABI описаны отдельно.

| Контракт | PC | PS2 |
| --- | ---: | ---: |
| Factory | `401A60` | `3F6BA0` |
| Constructor | factory path | `2F6D10` |
| Vtable | `6F6C7C` | `49B1B0` |
| Copy | `501F30` | `2F64B0` |

PS2 конструктор вызывает `wxProjectile` и обнуляет `EC`, `F0`, `F4`; PC снимок после factory даёт те же значения. Copy на обеих платформах вызывает объектный Copy напрямую, затем для ссылок `EC` и `F0` уменьшает 16-битный счётчик старой, увеличивает счётчик новой и переносит адрес, после чего переносит слово `F4`. Следовательно, общий payload `wxProjectile` этим Copy не копируется и новый `spActor` не создаётся. Изменение счётчиков выполняет общий код базового класса; доступ к внешним объектам даёт `wxProjectileHost`.

PC деструктор освобождает `F0`, затем `EC`, после чего вызывает деструктор общей базы. При нулевом счётчике внешнее удаление выполняет host. [WinxStingerProjectileTests](../../../Winx/Tests/wxStingerProjectileTests.cpp) проверяет constructor defaults, прямую родительскую границу Copy, порядок ссылок, clone и освобождение хвоста.

Проверены factory/RTTI/пустой clone и удаление в исходном PC executable; PS2 constructor, Copy и таблица функций сопоставлены по коду. Физика, уведомления, игровые эффекты и полный PS2 teardown пока открыты.
