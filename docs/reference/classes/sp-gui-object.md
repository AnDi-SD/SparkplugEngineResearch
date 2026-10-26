# spGUIObject

`spGUIObject` (`0x7339573D`) физически строится через `spEntity`
(`0x22875AA1`) на PC и PS2. [Общие исходники](../../../Sparkplug/Code/Sparkplug/spGUIObject.h)
восстанавливают известные операции привязки Node, истории collision groups,
enabled byte и базовой нотификации. Исходные имена методов и header не найдены.

Конструктор вызывает Entity с аргументом `1`; зарегистрированный factory
самого Entity задаёт `3`. Это отдельное поле Entity, а не GUI enabled flag.
GUI enabled по `+0x28` начинается с raw byte `1`. Сохранённый массив collision
groups пуст. PC extent `0x3C`, PS2 `0x38`: различается контейнер массива.
Padding и PC allocator word конструктор не инициализирует.

[PC layout](../../../Sparkplug/Analysis/PC/spGUIAbi.h) и
[PS2 layout](../../../Sparkplug/Analysis/PS2/spGUIAbi.h) отделены от переносимого
C++ ABI. Семантика Entity lifetime описана в [карточке Entity](sp-entity.md).

## Привязка Node

Объект удерживает Node через унаследованное поле Entity `+0x18`. При замене
ссылки сначала снимаются reverse subscriptions со старого Node и его прямых
детей, затем восстанавливается доступный префикс collision groups. Число
восстановленных значений равно меньшему из collision count и saved count.
После этого изменяется 16-битная strong reference: старая уменьшается, нулевой
результат вызывает deleting destructor, новая увеличивается перед записью
указателя. При одинаковых указателях reference count не изменяется.

Для нового Node subscriptions добавляются сначала детям, затем самому Node.
Каждый текущий collision group добавляется в конец сохранённого массива,
после чего соответствующий `spCollisionInfo` получает group `4`. Перед новой
привязкой массив не очищается.
Это относится и к повторной привязке того же Node.

В результате последовательность `A[1,2,3] → B[16,32] → null` оставляет
у B значения `[1,2]`: восстановление читает начало накопленной истории
`[1,2,3,16,32]`. Общий исходник сохраняет это поведение игры.

Destructor выполняет GUIManager cleanup, снимает subscriptions с Node и
детей, освобождает массив и передаёт teardown унаследованному Entity.
Сохранённые collision groups при уничтожении не восстанавливаются.

PC Node хранит owning vector указателей на
[`spCollisionInfo`](sp-collision-info.md): begin/end/capacity по `+0x68/+0x6C/+0x70`.
Изменяемое GUI слово `+0x18` принадлежит полю `spCollisionInfo::group`.
Общая реализация использует `spNode::GetCollisionForAnalysis` и
`spCollisionInfo::GetGroupForAnalysis/SetGroupForAnalysis` напрямую.

## Enabled и базовая нотификация

Enabled setter сначала сохраняет raw byte, затем вызывает у Node рекурсивный
substring search `GUICollision` с обоими флагами `1`. При найденном Node
всем его collision groups присваивается `4` как при включении, так и при
выключении. Это не операция изменения Node EnabledMask.

Нулевой исходный Node приводит оригинал к разыменованию. Portable операция
сохраняет preceding enabled write и сообщает об отсутствующей зависимости
через exception. Это явная host-защита, а не native failure result.

Базовый Notify обрабатывает `0x18` записью своего указателя в выходную ячейку
из payload, а `0x1C` повторяет базовый enabled setter с текущим byte.
Остальные коды базовый класс игнорирует.

Общий [portable notification record](../../../Sparkplug/Code/SparkBase/spNotification.h)
описывает восемь логических слов: code, три промежуточных слова, sender,
ещё одно промежуточное слово, payload и tail. Host pointer widths не являются
native 32-битным ABI. `spNotificationForAnalysis` — аналитическое имя record,
без нового зарегистрированного игрового класса.

Тот же header содержит общий исходник наблюдаемого PC helper `435D30`.
Он создаёт временное сообщение `[code,0,0,0,sender,0,payload,tail]` и при
ненулевом target синхронно вызывает его notification slot. Frame и payload
заимствуются на время вызова, без retain/release. Нулевой target пропускает
dispatch. Класс-владелец helper и исходное имя файла не установлены.

## Копирование и границы

Copy копирует Named name и enabled byte. Он не копирует Entity Node,
сохранённую историю и runtime manager state. Полный clone создаёт новый
объект через конкретный factory и вызывает виртуальный copy. При копировании
в существующий destination его Node и saved history остаются прежними.

Поиск выполняет общий [`spNode::FindNodeForAnalysis`](sp-node.md#поиск-по-имени).
Доступ к collision groups также принадлежит общим восстановленным классам.
Reverse subscriptions и GUIManager cleanup имеют явные host-boundaries.
Notification target по
Entity `+0x24` также передаётся внешним provider: его producer graph,
физический тип и lifetime не приписываются GUI facade. Синхронный dispatch
выделен в общий [spNotificationHost](../../../Sparkplug/Analysis/Host/spNotificationHost.h).

GUI использует эти общие данные и не создаёт отдельную реализацию менеджера
коллизий. Startup GUI, полное populated Entity/GUI teardown, PS2
notification/lifecycle и отображение экрана остаются отдельными незакрытыми
частями. Изменения графа во время lookup и неизвестные derived overrides
требуют отдельной квалификации.
