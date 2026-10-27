# spPCErrorManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPCErrorManager](../../../Sparkplug/Code/SparkplugPC/spPCErrorManager.h).

Статус: class/base IDs, factory allocation, stateless layout, обе vtable,
factory/clone/destructor, handler provider и полная severity presentation map
подтверждены. Прямой source-path string не найден; размещение в
`Code/SparkplugPC` inferred по platform boundary и соседним registrations.

## Identity и адреса

| Поле | Значение |
| --- | ---: |
| Class ID | `0x12162D8E` |
| Base | `spErrorManager / 0x660E40D8` |
| Handler / provider | `0x004C34B0` / `0x004C3580` |

Factory выделяет ровно `0x1024`, вызывает common constructor `0x00413680` и
только заменяет vptrs. Полей производного класса нет. Clone повторяет factory,
регистрирует пару и вызывает inherited empty copy slot.

## Handler

Provider возвращает адрес `0x004C34B0`. Handler сначала форматирует цепочку
через common `0x004137F0`, затем смотрит `spError::severity +0x14`:

| Severity | Title | MessageBox flags | console code |
| ---: | --- | ---: | ---: |
| `0` | `Information` | `0x40` | `0x0A` |
| `1` | `Warning` | `0x30` | `0x0E` |
| `2` | `Error` | `0x10` | `0x04` |
| `3` | `Fatal error` | `0x10` | `0x04` |
| прочее | `Error` | `0x10` | `0x04` |

Native вызывает импортированный `MessageBoxA`, затем common console/log route
`0x00413500`. Portable `ForAnalysis` сохраняет точную classification map, но
не открывает модальное окно: handler пишет только в Win32 debugger channel.
Это явно безопасная аналитическая подмена внешнего side effect, не заявление
об идентичности UI.

## Console и очередь ошибок

Отдельный console-only путь `004139C0` не вызывает этот handler. Он печатает
один указанный error, выбирая console code по severity головы очереди:
information `0x0A`, warning `0x0E`, остальные `0x0C`. Подробный текст имеет
суффикс `\n- <source> (<line>)`; затем внешний printf добавляет перевод строки.
Очередь и used bytes сохраняются, lazy handler остаётся неразрешённым.
Именно этот путь использует
[standalone shader loader](../../engine/materials/shader-manager-key.md#загрузка-одного-standalone-shader).

Общий `CreateErrorForAnalysis` представляет nonfatal `0040EFA0`:40байт error
занимают fixed data stack, новый error становится головой и наследует более
высокую severity предыдущей головы. Message хранится отдельно с NUL.
Data stack расходует ровно запрошенные байты, без дополнительного выравнивания.
Переносимая ownership-коллекция обеспечивает срок жизни до `ClearForAnalysis`;
она не воспроизводит native ABI объектов внутри byte stack. Fatal dispatch
и allocator overflow остаются неподдержанными входами этого bounded метода.

## Границы описания

### PC-адаптер уже существующего пустого manager

`HandleWarmEmptyPcForAnalysis` — явно аналитический PC-профиль общего
тела `spErrorManager` по адресу `0x413A90`, а не восстановленный метод,
принадлежащий производному классу. Вызов `virtual20(0,2,0,0)` на настоящем
PC manager попадает именно в это общее тело. Оно сначала записывает ноль
в глобальное слово `0x75AB9C` и лишь затем проверяет голову очереди.
Семантическое назначение слова остаётся неизвестным. При пустой очереди
native возвращает false; вызывающий путь индексирования `0x467300`
игнорирует этот результат перед проверкой ссылки.

Адаптер требует конкретный текущий `spPCErrorManager`: общий singleton
должен указывать на этот receiver, head и used должны быть нулевыми,
fatal/overflow — false. Явный профиль `StableBorrowed` означает живое
внешнее непересекающееся хранилище raw word и стабильные синхронные
manager/context на время вызова; он не проверяет чужое владение или
конкурентные изменения. Неизвестный/изменяемый контекст и неподходящее
состояние получают аналитический отказ до изменений.

После проверок адаптер очищает переданное raw word и непосредственно
вызывает общий `HandleForAnalysis(Error)`. Его пустой false игнорируется;
отдельный `WarmEmptyHandleStatusForAnalysis::Completed` обозначает
завершение этого ограниченного адаптера, не исходный bool. Очередь,
data stack и lazy handler при этом сохраняются; presentation callback
не вызывается. Даже начальное ненулевое raw word становится нулём.

Этот профиль не приобретает manager из native global `0x755264`, не
создаёт его, не реализует queued/modal/fatal route и не квалифицирует
lifecycle, PS2 или неустойчивые заимствованные графы. Существующие методы
индексирования Serializer сохраняют явный отказ: адаптер сам по себе
не восстанавливает рекурсивное индексирование.

Не найдены original TU/header, имя provider/callback typedef, точный контракт
`0x00413500`, owner imported MessageBox pointer `0x006D9404` и политика
headless/release builds.
