# KikoMoving: сообщение и два raw DWORD

PC-обработчик `wxKikoMovingAIAction` выбирается primary cell `04`.
Он получает адрес входа как единственный stack argument, читает ровно один
DWORD с offset `0` и сравнивает все 32 бита с `000027AA`.

| Входной DWORD | Изменение captured action |
| --- | --- |
| `000027AA` | DWORD `+3A8 = 3`, затем DWORD `+3AC = 2` |
| Любой другой | Все 7160 байт сохраняются |

Коды `000127AA`, `FFFF27AA` и `800027AA` не совпадают. Сравнение low byte,
low word или маскирование старших битов меняет поведение. Обе записи имеют
ширину четыре байта; весь prefix, остальные собственные bytes и padding
остаются прежними. Повтор одинакового сообщения снова выполняет обе записи.
При следующем вызове заново читается входной DWORD. Собственные
`3A8/3AC` не читаются: несовпадающий код сохраняет явно изменённые между
вызовами значения, а совпадающий снова выполняет обе записи.

Native класс входного record не установлен. Для выбранного метода достаточно
readonly четырёх байт, disjoint от receiver; lifetime принадлежит вызывающему.
Более длинная notification frame, pointer fields и производственная доставка
не следуют из этого чтения. Значения `3`, `2` и код `27AA` сохраняются как
наблюдаемые литералы без придуманного имени события или состояния.

Наш `wxKikoMovingAIAction` хранит captured7160B как инертные bytes отдельно
от portable `wxAIAction`. Его `HandleMessageForAnalysis` принимает существующий
portable `wxAIActionMessageForAnalysis` по значению; это наш четырёхбайтовый
value view, а не заявление о native message-классе или pointer ABI. Профиль по
умолчанию отказывает до изменения captured state. Явный PC-профиль требует
captured primary и пустого portable current/owned graph. Encoded pointers в
raw bytes не преобразуются в C++ указатели и не разыменовываются.

Generic `void*` notification hook остаётся unqualified: аналитический метод
не подключает production dispatcher. Отдельный bounded access trace описывает
одно чтение входа и ноль либо две упорядоченные записи; PCs, stack и native EAX
не являются этой portable трассой или возвращаемым `ResultForAnalysis`.
Native C++ return type остаётся неизвестным. Допуск не квалифицирует aliasing,
fault/rollback, reentry, concurrency, текущий constructor, владение graph,
lifecycle, все ветви движения, PS2, runtime или gameplay.

[Идентичность и PC layout класса](../../reference/classes/wx-kiko-moving-ai-action.md).
