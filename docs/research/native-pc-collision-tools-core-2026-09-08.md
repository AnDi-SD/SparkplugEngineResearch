# CollisionInfo и OBB: используемая инструментами часть PC

Проверенный блок цикла до 07:30 МСК 9 сентября. Источник — pristine PC
`WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Стенд: `research/probe_pc_collision_core.py`. Старые structural-corpus выводы
дополнены исполнением оригинальных методов; полный обход SMO не повторялся.

## Что восстановлено

| Класс | Нужная часть | Подтверждение PC |
|---|---|---|
| `spBoundingVolume` | реальный RTTI-base, физический named prefix, локальная ограничивающая сфера | registration `6D48DD`; ctor `491E50 →416510` |
| `spOBBBV` | параметры OBB, defaults, клонирование, изменение PR CollisionInfo | factory `4879C0 →13C7350`, ctor `4FDF30`, clone `487A20`, update `486930` |
| `spOBBBVSerializer` | полноценный reader/writer известных полей | `439BA0 /439E70` |
| `spCollisionInfo` | primitive/group/PRS/sphere, владельцы, clone, нужные world effects | ctor `465050 →4742A0`, factory `4653A0`, copy `464EF0`, update `4651E0`, destructor `465130` |
| `spCollisionInfoSerializer` | primitive/group/transform, связи и запись | reader `438A80`, index `438A40`, writer `438E20` |
| `spNode` | присоединение/отсоединение/клонирование CollisionInfo; update перед детьми | `421ED0 /421690 /421F80 /421420` |

Идентификаторы: CollisionInfo `47A97C0E`, BoundingVolume `21CC76AF`, OBBBV
`4DA04889`, CollisionInfoSerializer `33380E8C`. PS2 registration/factory
`4802D0 /124B90` дополнительно подтверждают CollisionInfo identity и размер
`0x88`; PC размер `0x8C`. Эти размеры не выдаются за ABI современного C++.

## Контракты, которые нельзя заменять удобными предположениями

- CollisionInfo по умолчанию: group `1`, primitive/node null, P=0, R=identity,
  S=1, sphere=0. Конструктор PC также лениво создаёт CollisionManager;
  игровой singleton/query subsystem не нужен представленному tools API.
- CollisionInfo удерживает primitive intrusive-reference в оригинале. Повтор
  той же ссылки не меняет счётчик; замена освобождает предыдущую.
- Node владеет CollisionInfo непосредственно: attach не увеличивает его
  intrusive count; detach удаляет перестановкой последнего элемента. В host
  используется общий владелец с FAT/context и единственная обратная ссылка
  Node. Двойное присоединение/reparent отвергается как опасный host input.
- CollisionInfo clone копирует group и разделяет прежний primitive. PRS не
  копируется: новый clone сохраняет constructor defaults. OBB clone использует
  inherited named copy и также оставляет геометрию конструктора.
- Reader переводит quaternion в matrix без нормализации. Проверен
  неединичный `(0,0,0.5,0.5)`; writer переводит полученную matrix обратно общим
  восстановленным методом, поэтому исходные quaternion bytes не сохраняются.
- Writer CollisionInfo пишет primitive при его наличии, затем **всегда** group
  и transform. Старые файлы с пропущенными полями не задают suppression writer-а.
- При Node update CollisionInfo сначала получает Node world PRS и вычисляет
  sphere. Затем OBB virtual меняет **переданные PR CollisionInfo**: добавляет
  повернутый локальный OBB position, умножает OBB orientation на входную.
  Scale не участвует в этом смещении или radius. Локальные OBB поля сохраняются.
- Без Node, а также для RTTI `spPartitionSystem` (`912CC341`), updater оставляет
  PRS CollisionInfo и пересчитывает только sphere. Второй вариант подтверждён
  статическим dispatch `465206..465212`; текущие runtime specimens покрывают
  обычный Node и отсутствие Node, не весь PartitionSystem.

## Проверки

Восемь micro-cases: defaults, transform, primitive, repeated replacement,
clone, Node update, rotated Node update, OBB read/write/clone. Все прошли;
каждый отдельно проверяет освобождение выделений. Профиль: 64 KiB guest arena,
100k instructions /2 s на вызов, 30 s на процесс. Leaf reference в field-only
случаях — явно заданная typed fixture; она не считается проверкой resolver-а.

Отдельно C++-writer сформировал полный трёхобъектный FFPS. Оригинальный
`422B50` загрузил его через реальные FAT/reference/reader/Node attach/update
методы, без подмены этих тел; совпали структура, позиции и bounds. **89 984
инструкции whole-load, 31/31 native allocation освобождены**, arena reservation
12 304 bytes. Свежий file profile был объявлен заранее: input≤4 KiB, 3 objects,
128 KiB arena,1M instructions /8 s на вызов,30 s outer. Это не продолжение
исторического остановленного micro-прогона другого Node graph.

C++: `spCollisionCoreTests.cpp`, 55 проверок с сохранением specimen, 54 без
выходного файла. Golden wire bytes и states взяты из перечисленных native
прогонов. Также прошли прежние AnimationKey/AnimationRuntime/NodeWorld/
TransformInput/SanReader suites. Новые записи и чтение используют общий
SectionCursor/spDataBlockSerializer, а все Node-derived serializers используют
один общий обход Node child/collision references.

## Граница готовности

Это готовая используемая часть CollisionInfo/OBB, **не весь collision runtime**.
API не предлагает contact/intersection queries, collision planes/corners,
scene broadphase или manager registration. Представлены только данные и
побочные изменения PRS/bounds, необходимые загрузке, просмотру и записи.
Null primitive при попытке update, нечисловые входы и двойное владение
отклоняются host boundary; оригинальное ошибочное разыменование не имитируется.

Наличие C++-классов ещё не означает миграцию Viewer/Importer и остальных SMO
потребителей. Открыты `spMeshBV`, прочие нужные BV/partition graph payloads и
общий интерфейс доступа приложений к ресурсам. Проверенный маленький FFPS не
объявляет весь corpus поддержанным или игру запущенной.

Результаты: `local-data/results/tools-core-cycle-20260909-0730/collision/`.
Хеши исходников и артефактов закреплены отдельным manifest этого блока.
