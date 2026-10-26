# spMessageQueue

Общие исходники: [spMessageQueue](../../../Sparkplug/Code/Sparkplug/spMessageQueue.h).
Имя класса и ID `C32F87A0` подтверждены; исходный путь файла и имена методов
неизвестны. Размещение в `Code/Sparkplug` отражает зависимость от времени задач.
Регистрационная и физическая база — `spBaseObject`.

Очередь встроена в PC `wxGameTimer+44` и две области `spEngineCore+CC/+110`.
На PS2 две очереди core находятся по `+C4/+108`. Это отдельный тип, а не
неизвестный успешный callback таймера.

| Состояние native queue | Смещение |
| --- | ---: |
| `spBaseObject` | `00–0F` |
| descriptor pool, размер `2C` | `10–3B` |
| голова односвязного списка | `3C` |
| borrowed source timer | `40` |

Native queue занимает `44` hex байта. Узел занимает `24` hex: сообщение из
восьми слов (`20` hex) и next по `20`. Pool начинается с четырёх блоков по
64 записи, размер записи — 36 байт; отдельные [PC](../../../Sparkplug/Analysis/PC/spMessageQueueAbi.h)
и [PS2](../../../Sparkplug/Analysis/PS2/spMessageQueueAbi.h) описания сохраняют ABI.
Portable storage — явно наша замена pool. Она сохраняет порядок и время жизни
сообщений, но не его размещение, повторное использование адресов и политику ошибок.

`EnqueueForAnalysis` сначала записывает в caller word04 сумму current+delay
по modulo2³², затем выделяет узел и копирует все восемь logical words.
Вставка использует unsigned due: новый узел идёт перед строго большим временем;
равные сроки сохраняют порядок поступления. Переполнение не превращается
в сравнение elapsed-time: малые получившиеся значения стоят в начале списка.

`TickForAnalysis` один раз читает source current по native timer+1C и обрабатывает
голову, пока её unsigned due≤captured now. Перед Notify голова уже снята,
а текущий узел остаётся жив до возврата callback. Затем узел возвращается
в storage и голова читается заново. Поэтому direct callback может Clear,
добавить сообщение или вложенно вызвать Tick. Вложенный Tick получает свой now;
изменение clock не меняет отсчёт внешнего вызова. Clear и destructor освобождают
ожидающие узлы без dispatch и не владеют receiver/sender/payload.

Word0C=0 вызывает receiver из native word14. Любой ненулевой ключ, включая
`80000000` и `FFFFFFFF`, вызывает настоящий общий `spSubscriptionManager`.
При отсутствующем singleton приложение через borrowed provider создаёт и хранит
реальный shared manager; Queue сама вызывает его Dispatch. Эта служба не заменяется
произвольным успешным обработчиком. Квалификация рассылки сохраняет неизменяемую
группу подписчиков; её изменение и recursive manager dispatch остаются открытыми.

Общий `spNotificationForAnalysis` и immediate-helper `435D30` сохраняют прежний
контракт: у immediate записи word14=0, отдельный target передаётся напрямую.
Для queue на 64-разрядном host caller предоставляет `{uint32 token, spBaseObject* receiver}`.
Token должен соответствовать packet14 и живому borrowed объекту. Queue вызывает
настоящий notification virtual этого объекта. Missing/mismatched mapping и
отсутствующий provider возвращают отдельный analytical refusal status; это
наши проверки, не native false или реконструкция null/fault.

Factory отсутствует, Clone возвращает null, inherited Copy не копирует очередь.
Динамически квалифицированы PC операции и direct callback mutations; PS2 здесь
сохраняет статический layout/порядок, без заявления общей EE/FPU эмуляции.
Открыты native allocator/failure/rollback, полная политика глобальных служб,
уничтожение самой очереди из callback и полная интеграция GameTimer/EngineCore.
