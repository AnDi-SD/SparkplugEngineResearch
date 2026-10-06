# wxSpiderAttackState

`wxSpiderAttackState` — состояние атаки паука, физически производное от
[`wxCharacterState`](wx-character-state.md). Class ID — `0x6ADD2466`, selector —
`3`; исходный объект PC и PS2 занимает `0x3C` байт и не добавляет поля к базе.
Переносимый C++ объект не имитирует исходный ABI.

Вход устанавливает скорость consumer в `1.0`, затем читает байт `+0x20`
контроллера владельца. Нулевой байт выбирает ключ `(key & 0xF0000000) | 0x480`,
любой ненулевой — `(key & 0xF0000000) | 0x400`. После поиска анимации состояние
сбрасывает соответствующие completion records, запускает найденный handle с
`mode=false`, `interrupt=true` и fade selector `0` либо `2` по предикату
владельца. Оно делает это также для нулевого или прежнего handle. Старый pending
не освобождается. Новый pending записывается после запуска; виртуальный update
обнуляет слово `+4` контроллера действий владельца.

Permission возвращает `true` для нулевого pending без обращения к consumer.
Для ненулевого handle оно вызывает consuming completion query. Hook `0x38`
наследует `false`; остальные transition/reset методы принадлежат базе.

Event hook сравнивает полные строки с учётом регистра:

| Тег | Действие |
| --- | --- |
| `event_impact_begin` | Уведомление `0x271F` получателю `owner +0x24`, флаг `true` |
| `event_impact_end` | То же уведомление, флаг `false` |
| `event_web` | Действие `0` объекта атак владельца |
| `event_spit` | Действие `0` того же объекта |

Нулевой получатель пропускает уведомление. В нативном bool payload записан
только младший байт; старшие три байта — неинициализированное выравнивание.
Объект атак находится по PC `owner +0x124 → +0x140`, PS2
`owner +0x130 → +0x14C`. Полные графы этих объектов и backend consumer не
восстановлены этим компонентом; требуются явно привязанные внешние службы.

Конструктор, factory и clone используют тип состояния. Copy — пустой
наследованный метод: clone получает начальное состояние, а Copy не переносит
текущий pending или flags. Reset выполняет исходный базовый сброс.

Реализация: [wxSpiderAttackState.h](../../../Winx/Code/wxSpiderAttackState.h),
[wxSpiderAttackState.cpp](../../../Winx/Code/wxSpiderAttackState.cpp). Наш
[host](../../../Winx/Analysis/Host/wxSpiderAttackStateHost.h) отделяет чтение
внешнего графа, анимационный consumer и доставку уведомлений. Точные адреса и
размеры: [PC](../../../Winx/Analysis/PC/wxSpiderAttackStateAbi.h),
[PS2](../../../Winx/Analysis/PS2/wxSpiderAttackStateAbi.h). Проверка:
[wxSpiderAttackStateTests.cpp](../../../Winx/Tests/wxSpiderAttackStateTests.cpp).
