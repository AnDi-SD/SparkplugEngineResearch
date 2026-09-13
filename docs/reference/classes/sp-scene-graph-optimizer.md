# spSceneGraphOptimizer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spSceneGraphOptimizer](../../../Sparkplug/Code/Sparkplug/spSceneGraphOptimizer.h).

## Иерархия и platform split

Общий `spSceneGraphOptimizer` имеет class ID `0x4FE639C2`, direct registered
base `spCrossPlatform` (`0x20A72504`) и null RTTI factory на обеих платформах.
Это согласуется с callback-абстракцией: portable класс не создаётся через RTTI
и требует derived callbacks.

| Факт | PC | PS2 |
| --- | ---: | ---: |
| Optimize / OptimizeNode | `0x004C1900 / 0x004C19D0` | не слинкованы в доступном executable |

Конкретный `spDXSceneGraphOptimizer` (`0x7E120EC3`) зарегистрирован только на PC:

Original header/TU не найдены; `Code/Sparkplug/spSceneGraphOptimizer.*` — inferred путь.

## Доказанный общий проход

PC `0x004C1900` выполняет проход в следующем порядке:

1. очищает временный vector по `+0x24..+0x2C`;
2. вызывает `OnStartOptimize(pNode)` через callback subobject `+0x18`;
3. вызывает virtual `OptimizeNode(root, o_iCurrentNode)`;
4. проходит временный список пустых render-листьев и отсоединяет имеющие
   parent узлы;
5. вызывает `OnEndOptimize()` даже при ошибке traversal;
6. очищает второй временный контейнер и возвращает traversal result, если
   end callback успешен.

Если `OnStartOptimize` вернул false, traversal и `OnEndOptimize` не вызываются.
Имена и error contracts подтверждены строками executable:
`OnStartOptimize`, `OnEndOptimize`, `OnNode`, `OnModel` и `OptimizeNode`.

`0x004C19D0` сначала вызывает `OnNode`. Для `spRenderNode` он проходит
renderable vector `+0xBC..+0xC0`, фильтрует `spModel` (`0x763277DB`) и вызывает
`OnModel(renderNode, model)`. Затем рекурсивно обходит child list. Только после
успешного обхода узел без children и renderables заносится в deferred detach
list; поэтому за один pass удаляется исходный слой пустых листьев, а не вся
вновь образовавшаяся пустая цепочка.

Аргумент, названный диагностикой `o_iCurrentNode`, общий body только передаёт
в рекурсию: собственного чтения или записи не обнаружено. Portable код поэтому
не придумывает счётчик.

## Vtable и DX callbacks

Общая PC primary table имеет десять slots:

`4C1D80, 5B7A00, 4A1BF0, 413120, 4C1CC0, 408350, 408370,
4C1900, 4C19D0, 5A7DB0`.

DX primary table сохраняет три последних common slots, а identity/lifetime
заменяет leaf-реализациями. Отдельная callback table:

`4F3DF0, 4BFA10, 4BEEE0, 5A7DB0, 4C0100`.

## Portable срез и открытая граница

Portable `spSceneGraphOptimizer` сохраняет null factory, direct base,
callback order, model filter, recursion, failure propagation и deferred detach.
Callbacks оставлены abstract. Host vectors/shared ownership не претендуют на
native ABI. Общий destructor/Optimize фиксирует observed prefix `0x38`:
singleton/callback/support vptr `+0x14/+0x18/+0x1C`, temporary vector
`+0x20..+0x2C` и detach-list `+0x30/+0x34`. Это нижняя доказанная граница,
не exact `sizeof`.

DX `OnNode` независимо уточняет concrete prefix до `0x54`. Все обращения идут
от callback subobject `complete+0x18`: `callback+0x28` даёт list sentinel
`complete+0x40`, а `callback+0x34` — group tree/map sentinel `complete+0x4C`.
По стандартному layout тех же old-MSVC контейнеров получаются list state
`+0x3C/+0x40/+0x44` и map state `+0x48/+0x4C/+0x50`. Это всё ещё нижняя
наблюдаемая граница: protected factory не показывает exact allocation size.

Три save-side entry теперь разведены по владельцам: `0x004BEDF0` вызывается на
optimizer и возвращает `spDXCombinedVB` для меша; `0x004C07E0` вызывается уже
на самом combined-VB и возвращает пять range-слов; `0x004C0540` вызывается для
каждого элемента вложенных grouping lists как `CombineData()`. Все три тела,
как и core `OnEnd`/destructor/add-mesh, являются SecuROM `.rld` VM-thunk-ами.
Статический анализ честно фиксирует контракт и окружающие контейнеры, но не
выдаёт VM bytecode за декомпилированный C++.

Открыты: точные common/DX `sizeof`, original paths/API, singleton owner, семантика primary slot `+0x24`, payload grouping-record-ов, DX add/group policy, тела защищённых lookup/CombineData, deduplication, failure rollback и device-lifetime.
