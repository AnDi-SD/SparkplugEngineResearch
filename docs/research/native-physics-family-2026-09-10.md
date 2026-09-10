# Физические объекты и BoundingVolume: PC и PS2

[Контракт](../../research/physics-family-contracts-2026-09-10.json) связывает
pristine binaries, exact windows, cached catalog и версии проб. Результаты:
`local-data/results/native-cycle-20260910-1900/physics-family/`.
Это исследование следующего семейства целых executable, а не новый physics
backend инструментов.

## Состав и создание

| Класс | PC размер | PS2 размер | Подтвержденный объем |
|---|---:|---:|---|
| spBoundingVolume | factory 0 | factory 0 | RTTI, null Clone, физический Named parent |
| spCapsuleBV | 120 | 120 | PC полный lifetime и geometry Clone; PS2 ctor/RTTI prefixes |
| spConvexBV | 48 | 48 | тот же объем, отдельные собственные поля |
| spCollisionManager | 6032 | 6032 | PC полный lifetime; PS2 independent factory/table/getter |
| spConstraint | factory 0 | нет регистрации | PC table/getter/null Clone, base destructor |
| spConstraintSystem | 16 | нет регистрации | PC полный lifetime |
| spContactConstraint | 228 | нет регистрации | PC полный lifetime и реальная manager registration |
| spRigidBody | 312 | нет регистрации | тот же объем, другой manager registry |
| spPhysicsManager | 132 | нет регистрации | полный lifetime и владение двумя populated registries |

Еще четыре BV (Box, Mesh, OBB, Sphere) входят в выбранную closure. Их восемь
прежних PC/PS2 оценок сохранены. PS2 getter/table identity повторно сопоставлена
в общем небольшом проверочном блоке; новых полных lifetimes для них не запускали.
Отсутствие зарегистрированных PS2 классов Constraint/RigidBody/PhysicsManager
не означает, что весь collision engine PS2 отсутствует.

**Семь новых PC lifetimes, 35 операций:** factory, RTTI, Clone, два удаления.
ContactConstraint и RigidBody на первом проходе дошли до импортного
MSVCR71.memmove (IAT 006D9300). Повторный свежий проход с существующей общей
`pc_crt_memory_fixtures` прошел. Игровой remove/erase исполнялся оригинальный;
новой копии этой логики не вводилось.

## Владение PhysicsManager

`probe_physics_family_contracts.py`: три populated ownership случая, два
geometry Clone и двенадцать PS2 случаев; **1,617 с**.

Оригинальная factory 00459F00 создает PhysicsManager и устанавливает global
0075DB80. Создание RigidBody factory 0049BAC0 добавляет его в собственный
registry; ContactConstraint factory 0048BCF0 — в другой. Проверены:

- три RigidBody, порядок удаления middle → first → last;
- три ContactConstraint, удаление с конца;
- два тела и два ограничения, перемежающиеся удаления.

Body vector begin/end/limit расположен по +2C/+30/+34, constraint vector —
по +3C/+40/+44. После удаления порядок остальных элементов сохраняется.
Удаление из середины действительно исполняет native erase и импортный memmove.
Сравнивается весь 132-byte manager, а не только счетчик элементов.

Создание тела также устанавливает byte +21 в 1; удаление не сбрасывает его.
Название и дальнейшее назначение флага пока не приписывается. Остальные байты
manager вне двух vector headers и этого флага сохраняются. После удаления
всех объектов пустой manager освобождает свои буферы и обнуляет global.
Все allocations каждого опыта освобождены. Это устанавливает владельца
двух оставшихся context allocations в исходных cold lifetimes тела/ограничения.

Привязка ограничений к телам, ненулевые внешние references и физический solver
этими опытами не закрыты.

## BoundingVolume и геометрия Clone

Зарегистрированный parent BoundingVolume — spBaseObject. PC физический Named
prefix уже доказан прежним [collision-core исследованием](native-pc-collision-tools-core-2026-09-08.md).
Теперь PS2 независимо показывает constructor 00127EC0 → 00105F60:
последний записывает table 0048C690, чей getter точно возвращает record
spNamedObject. Это не повод менять зарегистрированную ancestry.

Base object tables: PC 006ECACC, PS2 0048D1C0. Base Clone возвращает null
004A1BF0 /00127F00. Concrete Capsule/Convex имеют собственные nonnull Clone;
их virtual Copy — inherited Named copy 00413120 /00105DC0.

PC Capsule и Convex были изменены после реального создания. У Capsule менялись
числовые поля +28/+2C, часть matrix +30/+34/+38 и +6C/+70/+74;
у Convex — type +14 и count +2C, при null payload pointer. После оригинального
Clone вся allocation клона совпадает с factory snapshot, а измененный источник
сохраняется. Оба объекта затем удалены без остатка. Это проверка конкретных
собственных полей, а не утверждение, что Named metadata вообще не копируется.

PS2 Capsule factory 00129900 выделяет 120, вызывает base 00127EC0,
устанавливает table 0048D220 и единицы float по +28/+2C; matrix constructor
00109470 получает this+30 и аргумент 1. Отдельный post-segment prefix
0012998C обнуляет +6C/+70/+74 и ставит type +14 =3. Полные matrix/segment
constructors в этой PS2 пробе не исполнялись.

PS2 Convex factory 00129FB0 выделяет 48, вызывает тот же base, затем ставит
table 0048D280, type +14 =6 и нули +28/+2C. Свежий ограниченный prefix
исполняет эти записи до LQ-эпилога. Сравнивается вся заданная storage область.
Полный populated PS2 Clone и R5900 collision/FPU pipeline не заявляются.

PS2 CollisionManager factory 00126ED0: allocation 1790 hex, base 00102BF0,
object table 0048D170 и дополнительный singleton interface по +10;
далее реальные array/manager initialization calls. Это static construction
identity с runtime getter, не полностью исполненная PS2 factory.

## Исправления стенда и оценка

Первый совмещенный probe после успешных PC опытов ошибся в диапазоне PS2
global data: mapping с 004A0000 не включал GP-496C =0049F804. Второй использует
явно заданные шесть страниц 0049F000..004A5000. Прежний source и failed report
сохранены. Scout окно с подписью `pc-constraint-constructor-entry` по 0048B290
на деле начинается в предыдущей math routine; оно исключено из атрибуции
методов класса. Игровые инструкции из-за этих ошибок не менялись.

Новые PC строки: BoundingVolume 35 (reviewed-existing, первый учет ранее
изученного), Capsule 35, Convex 30, CollisionManager 25, Constraint 15,
ConstraintSystem 20, ContactConstraint 30, RigidBody 30, PhysicsManager 35.
Новые PS2: BoundingVolume 25, Capsule 25, Convex 25, CollisionManager 20.
Остальные восемь оценок сохранены. Новых полностью закрытых классов нет.
Открыты точные collision queries, solver, динамическая интеграция, attached
graphs, populated CollisionManager и соответствующие PS2 math реализации.
