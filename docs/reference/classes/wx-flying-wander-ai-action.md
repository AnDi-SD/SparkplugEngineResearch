# `wxFlyingWanderAIAction`

`wxFlyingWanderAIAction` (`7529750D`) физически и регистрационно наследует
[`wxAIAction`](wx-ai-action.md). Native размеры — PC `3E0`, PS2 `3E4`.
[Исходник](../../../Winx/Code/wxFlyingWanderAIAction.cpp) хранит общий контракт,
[PC](../../../Winx/Analysis/PC/wxFlyingWanderAIActionAbi.h) и
[PS2](../../../Winx/Analysis/PS2/wxFlyingWanderAIActionAbi.h) layouts отделены
от переносимого объекта.

## Собственные поля и Node

PC constructor `5B5220` дважды читает raw triple из mutable global `7600E0`:
первый набор в `3A8/3AC/3B0`, второй — в `3C0/3C4/3C8`. Между ними он
устанавливает `3B4 = 300.0f`, обнуляет `3B8/3BC`; затем обнуляет
`3CC/3D0/3D4` и byte `3D8`. Семантическое имя этой global triple и остальных
полей не присваивается по догадке. Заимствованный data input передаётся через
[host](../../../Winx/Analysis/Host/wxFlyingWanderAIActionHost.h); это не
универсальная константа нулевого вектора.

Constructor создаёт настоящий `spNode`, сохраняет собственную reference и
прикрепляет Node к существующему root через общий Node Attach. Сам Node
сохраняет обычные constructor position `0`, scale `1` и самостоятельное
состояние: изменённая action default triple не меняет его constructor defaults.

PC destructor `5B5350` сначала удаляет Node из текущего root, затем отпускает
собственную reference, обнуляет `3DC` и переходит к общей AIAction базе.
При собственных двух references — action и root — Node удаляется. Если
внешний потребитель удерживает ещё одну reference, Node переживает action с
null parent и удаляется после последнего внешнего Release.

Переносимая реализация использует настоящий общий `spNode::AttachChildForAnalysis`
и `DetachChildForAnalysis`, а native 16-битный reference counter представляет
через `std::shared_ptr`. Это наша техническая замена представления владения,
без претензии на native memory overlay, overflow или произвольный refcount.
Для PC lifetime нужен заранее построенный стабильный root с null Scene link,
который вместе с host переживает action. Ненулевая Scene registration,
холодное создание core/scene, allocation failure и native rollback не закрыты.

## Hooks и Copy

Все три bool update slots используют literal true; собственных успешных
заглушек обработчиков здесь нет. Notify и Exit — no-op без common forwarding
и очистки базового runtime word. Entry обнуляет только `3B8/3BC`.

Query сначала проверяет owner permission byte `14D` на PC (`15D` PS2).
Нулевой byte пропускает Perception. При ненулевом byte вызывается настоящий
общий [`wxPerception::FindNearestForAnalysis`](wx-perception.md); ненулевая
цель запрашивает selector key `2`, parameter `0`. Собственные поля action
при Query не меняются. Численная область задаётся явно: конечный PC nearest64
или ограниченные точные целые координаты для PS2.

Copy `5B5140` выполняет общий base Copy, затем переносит двенадцать raw words
в порядке `3D0,3D4,3B8,3BC,3A8,3AC,3B0,3B4,3CC,3C0,3C4,3C8`.
**Byte `3D8` и owned Node `3DC` не копируются**: destination сохраняет свои
значения. Clone строит отдельный новый Node, прикрепляет его к root, сохраняет
constructor byte `0` и только затем копирует двенадцать слов.

Полный PS2 constructor/lifetime и Copy base, inherited pathfinding,
selector delivery, serializer reachability и whole gameplay остаются открытыми.
Наличие всех ограниченных own PC hooks не объявляет весь класс или AI готовыми.
