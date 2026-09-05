# `spPCErrorManager`: Win32 presentation leaf

Статус: class/base IDs, factory allocation, stateless layout, обе vtable,
factory/clone/destructor, handler provider и полная severity presentation map
подтверждены. Прямой source-path string не найден; размещение в
`Code/SparkplugPC` inferred по platform boundary и соседним registrations.

## Identity и адреса

Контрольный `WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

| Поле | Значение |
|---|---:|
| Class ID | `0x12162D8E` |
| Base | `spErrorManager / 0x660E40D8` |
| Registration / initializer | `0x00764770` / `0x006D5760` |
| Primary / support vtable | `0x006F255C` / `0x006F2558` |
| Getter / support thunk | `0x004C33A0` / `0x004C33B0` |
| Factory | `0x004C33C0` |
| Deleting / base destructor | `0x004C3430` / `0x004C3450` |
| Clone | `0x004C3460` |
| Handler / provider | `0x004C34B0` / `0x004C3580` |

Factory выделяет ровно `0x1024`, вызывает common constructor `0x00413680` и
только заменяет vptrs. Полей производного класса нет. Clone повторяет factory,
регистрирует пару и вызывает inherited empty copy slot.

## Handler

Provider возвращает адрес `0x004C34B0`. Handler сначала форматирует цепочку
через common `0x004137F0`, затем смотрит `spError::severity +0x14`:

| Severity | Title | MessageBox flags | console code |
|---:|---|---:|---:|
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

## Реконструкция и открытое

Добавлены `Sparkplug/Code/SparkplugPC/spPCErrorManager.h/.cpp`, exact leaf ABI,
addresses, registration/factory/clone и тест всех четырёх mapping branches.
Изолированные тесты проходят 2/2.

Не найдены original TU/header, имя provider/callback typedef, точный контракт
`0x00413500`, owner imported MessageBox pointer `0x006D9404` и политика
headless/release builds.
