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

## Границы описания

Не найдены original TU/header, имя provider/callback typedef, точный контракт
`0x00413500`, owner imported MessageBox pointer `0x006D9404` и политика
headless/release builds.
