# wxGameFlowState

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

## Identity и границы

| Факт | PC | PS2 |
| --- | --- | --- |
| Class/base ID | `11521AFA /415352A1` | те же |
| Base | `spBaseObject` | `spBaseObject` |
| Set three fields | `5D6850` | `33C230` |
| Get three fields | отдельный адрес пока не установлен | `33C260` |
| Reset global table | `5D6880` | `33B590` |
| Следующий большой метод | `5D6E70` | `33B910` |

## Конструктор, владение и clone

Destructor удаляет owned timer`2C` через его virtual deleting destructor,
обнуляет`2C`, вызывает base cleanup. Borrowed`30` не освобождается. PC wrapper
освобождает память состояния при `flags &1`. PS2 освобождает её при
**положительном signed16 значении аргумента** и отдельно допускает null this.
Разница native ABI сохранена; это не общий boolean-контракт.

## Листовые операции

Шесть собственных virtual hooks базового класса — действительные пустые методы:
два возвращают true, четыре выполняют return без значения. На PC это slots
`1C..30`, addresses `5A7DB0 /5B7A00`; на PS2 slots`24..38`, addresses
`33C2D0 /33C2C0 /33C2B0 /33C2A0 /33C290 /33C280`. PS2 имеет два header words
перед первым function pointer. Все шесть PS2 leaves исполнены. Для void hooks
случайное содержимое return register не получает семантического значения.

## Общая таблица 56 STX имён

PS2 reset имеет другую раскладку исходных globals и stack spills, но 56 записей
в`4C40F0..4C41CF`. Строгая straight-line provenance прослеживает каждый global
load до store и аргументы единственного вызова `4076A8(4C41E0,0,E0)`.
Это статический анализ, **не запуск PS2 reset**. Callee использует EE/MMI;
generic MIPS decoder не выдаётся за исполнение этого пути. Его full arbitrary
input behavior отдельно не закрыт.
