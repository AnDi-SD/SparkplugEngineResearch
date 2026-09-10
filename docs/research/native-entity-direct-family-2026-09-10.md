# wxEntity и прямые связи регистрации: создание, копирование, раскладка

Независимые каталоги PC и PS2 содержат одинаковые 71 строки: `wxEntity` и
70 типов с зарегистрированной базой `wxEntity`. У 67 есть ненулевая factory;
`wxCharacterStateMachine` уже исследован, его результат переиспользован.
Для остальных **66 новых классов PC factory и RTTI вернулись**, 64 default
clone вернулись, **61 полный lifetime** прошёл. Всего 318 новых class operations.
PS2 независимо подтверждает 67 factory sizes, собственные constructor windows,
vtable/getter identities. Это статический анализ PS2, не исполнение конструкторов.

Все имена, размеры, первые семь общих interface slots, исходные snapshots,
constructor calls/stores и история остановок находятся в
[`entity-direct-construction-contracts-2026-09-10.json`](../../research/entity-direct-construction-contracts-2026-09-10.json).
Включены управление персонажем и анимацией, загрузка моделей, AI поведения,
звук, HUD, снаряды, ловушки, головоломки и объекты окружения. Поведение этих
подсистем не объявляется изученным полностью на основании создания объекта.

## Регистрация и реальная цепочка конструкторов

Обнаружено пять расхождений, независимо подтверждённых в обеих платформах:

| Класс | База в регистрации PC/PS2 | Наблюдаемая цепочка конструкторов |
|---|---|---|
| `wxProjectile` | `wxEntity` | `spBaseObject → wxProjectile` |
| `wxKickableIce` | `wxEntity` | `wxEntity → wxGenericTrigger → wxKickableIce` |
| `wxRockProjectile` | `wxEntity` | `wxEntity → wxGenericTrigger → wxRockProjectile` |
| `wxSavePoint` | `wxEntity` | `wxEntity → wxGenericTrigger → wxSavePoint` |
| `wxStellaRingTrigger` | `wxEntity` | `wxEntity → wxGenericTrigger → wxStellaRingTrigger` |

Оригинальная регистрация Projectile: PC `006CDFD0..006CDFF5`, PS2
`004880D0..00488104`; обе явно используют wxEntity hash `796A1869` и его
registration object. Это не ошибка разрешения имён в каталоге.

PC factory-only probe для шести классов наблюдает реальные записи vtable
в объект. Тип каждой промежуточной таблицы проверен через её собственный
RTTI getter, не через соседство адресов. Все шесть прошли за 1,535 с.
Projectile пишет `spBaseObject` в `00425666`, затем собственную таблицу
в `013BF813`. Его размер `EC` на обеих платформах, тогда как wxEntity имеет
`124` на PC и `130` на PS2. PS2 Projectile ctor `002C5030` вызывает
`00102BF0`, который устанавливает таблицу `0048C5E0`; getter `00102830`
возвращает registration `0049FF60`, то есть **spBaseObject**.

Четыре других PC объекта между wxEntity и собственной таблицей записывают
`007024B0` в `00590136`: это **wxGenericTrigger**. На PS2 они сначала вызывают
`003AD760`, который вызывает wxEntity ctor и записывает `00498030`; getter
`003F8E70` возвращает wxGenericTrigger record `004C9170`. Остальные 61
производных PS2 конструктора непосредственно вызывают `00286CA0`.

У wxGenericTrigger нет зарегистрированной factory, однако его промежуточный
конструктор действительно существует. Нулевой factory сам по себе не
доказывает абстрактность C++-класса. Отдельный размер выделения и полный
собственный lifetime здесь не установлены. `wxBaseAIBehavior`, `wxMoviePlayer`,
`wxPerceptionTrigger` также имеют нулевые factory, их создание здесь не оценено.

**Следствие:** дерево runtime-регистрации и физическую цепочку конструкторов
нужно хранить отдельно. Регистрацию игры не исправляем и автоматически
генерировать C++-наследование по ней нельзя. Новые игровые классы в этом
блоке не реализовывались; решение не скрывает расхождение за адаптером.

## Общая база и ограничения раскладки

wxEntity: PC factory `00401110`, ctor `004DA7C0`, vtable `006F4EB8`, getter
`004DA3C0`; PS2 factory `003F8570`, ctor `00286CA0`, vtable `0049B8D0`, getter
`003F9A20`. Обе версии вызывают собственный spEntity ctor, создают встроенные
объекты и регистрируют entity в двух менеджерах. Подтверждены defaults:
bytes `28=0`, `38=39=1`, word `3C=4B095440`, `40=500`, `44=43160000`,
byte `48=0`, words `4C/50/54/74/78/7C/80/84=0`, byte `88=1`.
Назначение остальных полей нельзя выводить только из их начальных значений.

Конструкторы заполняют два блока по 16 слов максимальным конечным float32
`7F7FFFFF`: PC с `8C` и `CC`, PS2 с `90` и `D0`; два блока по три слова —
PC с `10C/118`, PS2 с `110/11C`. Это не нулевые матрицы. Различие размеров
в наборе составляет 0/8/12/16/20/24/28/32/36 байт; единого offset delta нет.
`wxCharacterMoveCtrl` имеет PC `1C18`, PS2 `1C30`.

## Остановки и исправления исследовательского стенда

Первый batch65 после wxEntity pilot: 51 полный lifetime за 30,570 с при трёх
workers. Семь fresh runs после сохранённых micro caps использовали явно
заданный `protected-block` profile; за 5,874 с вернулась factory
`wxProjectileManager`, а шесть teardown дошли до отсутствующей сцены.
Повышение лимита не заменяет игровой код. Block entries не считаются
инструкциями или покрытием исполняемого файла.

У `wxCollisionDetector` и `wxCylindricalDoor` clone вызвал ещё не подключённый
CRT import. PE import table независимо даёт IAT `006D9328` → MSVCR71.dll
`strncpy`, lookup RVA `0033D03C`; IAT `006D9320` → `strncat`.
Подключена существующая ограниченная платформенная fixture. В ней исправлено
чтение source за длиной n: отсутствие NUL в первых n байтах разрешено,
n=0 не читает source. Пять направленных regression cases: старая реализация
проваливает четыре, исправленная проходит все за 0,666 с. Первый запуск
самого теста имел неверный callee-pop и сохранён отдельно; исправлена только
объявленная calling convention теста. Оба class lifetime затем прошли за
0,538/0,633 с. Это исправление стенда, не восстановленного алгоритма игры.

Для `wxHudMeters`, `wxKickableIce`, `wxLabelDisplayText`, `wxRockProjectile`,
`wxSavePoint`, `wxStellaRingTrigger` остановки `0040FB60` требуют именно
**core global `00755274` → scene `+18`**, а не entity `+24`. Вызовы видны
в `0055E02B`, `00590496`, `0055F311`, `0059024E`. С прежним явно описанным
borrowed empty-scene context все шесть lifetime прошли за 12,218 с.
Игровой unsubscribe выполняется; подставного результата этого вызова нет.
Пустые core/scene записи не выдаются за исполнение игрового startup.

Остаются пять незавершённых путей:

| Класс | Что прошло | Точная остановка / отсутствующий контекст |
|---|---|---|
| `wxCharacterMoveCtrl` | factory, RTTI | clone → `004E6638`, чтение объекта через receiver `+18`; нормальная привязка ещё не выполнена |
| `wxProjectile` | factory, RTTI | clone → `00405C22`, null receiver у списочного helper; происхождение требуемого владельца ещё исследуется |
| `wxAudioListener` | factory, RTTI, clone | delete-clone → `0044FAA0`, null receiver при чтении `+70`; нужен исходный аудиоконтекст |
| `wxBreakableBarrel` | factory, RTTI, clone | delete-clone → `00541E9E`, entity `+18` null перед чтением `+2C` |
| `wxHUDPart` | factory, RTTI, clone | delete-clone доходит до `0055C217`, global `0075DB68` null перед чтением `+1C` |

Исходные неуспешные runs и PS2 окна соответствующих copy/destructor сохранены.
Сходство конечного адреса ошибки не доказывает одинаковую зависимость.
Недостающий контекст не заменён нулями или успешными fake game calls.

## Учёт

66 новых callable классов: PC score20 для 61 полного lifetime, score15 для
трёх остановок удаления, score10 для двух остановок clone; PS2 score15 за
независимые статические контракты. wxGenericTrigger получает отдельные10/10
за промежуточный конструктор и identity. Предыдущий CharacterStateMachine
не повышается. Итого 134 первых platform assessments; ни один класс не
объявляется полностью закрытым. Активные алгоритмы, изменённый source для
copy, полноценная сцена, HUD/audio lifecycle и нормальное связывание открыты.
