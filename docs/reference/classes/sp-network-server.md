# spNetworkServer

PC-компонент сетевого приёма и диспетчеризации Sparkplug, class ID
`7259443E`, регистрационная и физическая база `spBaseObject`. Общие
[исходники](../../../Sparkplug/Code/Sparkplug/spNetworkServer.h) восстанавливают
указанные ниже операции, включая bounded worker, TCP admission и dispatcher.
Первоначальная конфигурация и заполнение ID pool, живые потоки и готовая
серверная сессия ещё не установлены; игровой multiplayer не заявляется.
PS2 counterpart этим компонентом не устанавливается.

## Идентичность и хранение

| Операция | PC entry |
| --- | --- |
| Factory / constructor | `00484290` / `00483CE0` |
| Clone / destructor / deleting destructor | `004842F0` / `00484340` / `004847A0` |
| Open / Close | `00484820` / `00483DE0` |
| SetQueue / SetPeer | `004847C0` / `004838E0` |
| FindPeer / CountOwnedPeers | `004833C0` / `00483480` |
| PruneDisconnected / RemovePeer | `00483520` / `00483EC0` |
| Broadcast / ReleaseId | `00483360` / `00483C20` |
| AllocateId / его тело | `00483850` / `00492D90` |
| NewTcpConnection / InitialInfoRequest | `00483930` / `00483A60` |
| InitialInfoReply / Ready / HelloRequest | `00483110` / `00483610` / `00483740` |
| ProcessPackets / Worker / thread callback | `00483F30` / `00484520` / `00484800` |

Размер оригинального объекта — `44` hex. Vtable — `006EC04C`, RTTI record —
`00761758`. [PC layout](../../../Sparkplug/Analysis/PC/spNetworkServerAbi.h)
отделён от переносимого C++ объекта.

| Offset, hex | Состояние и смысл |
| --- | --- |
| `10` | Указатель на строку `Network Server`; это не вторичная vtable |
| `14` | uint16 ID, исходно `BAD0`; ноль включает слушающий TCP путь |
| `16` | uint16 верхняя граница ID, конструктором не записывается |
| `18..20` | Список uint16 ID; allocator word `18` не записывается |
| `24` | Объект thread, исходно создаётся factory `006BE5C0` |
| `28 / 2C` | Принадлежащие объекту TCP/UDP SocketStream, type `1 / 0` |
| `30 / 34` | Выделенный list container peer pointers и lock |
| `38` | Queue pointer, исходно null; особое владение при замене |
| `3C` | MemoryStream, открытый с именем `Packet buffer` |
| `40` | uint32 счётчик принятых соединений, исходно ноль |

Переносимая верхняя граница представлена `optional<uint16_t>`: отсутствие
значения сохраняет неизвестное исходное состояние. `SetStateForAnalysis`,
`SetIdForAnalysis` и передача `unique_ptr` в `AdoptPeerForAnalysis` — наши
явные средства задания состояния и владения. TCP admission вызывает тот же
переносимый путь владения после фактического принятия stream в общий Peer.
Getter состояния и списка peers требует внешней синхронизации.

## Открытие, закрытие и thread

Open при ID `0` открывает TCP stream в режиме `1`, затем открывает UDP в
том же режиме при любом ID. Ошибка любого шага возвращает false без отката
уже открытых соединений. После этого вызываются thread Create с token
`00484800` и телом server, затем Resume. Оба bool проверяются. Фабрика
платформы должна связать token с допустимым callable и общим
`RunWorkerForAnalysis`; автоматический запуск OS thread здесь не добавляется.

Оригинальный callback получает **thread object**, читает из него `+18` и
вызывает worker `00484520` с полученным server receiver. Это согласуется с
[spThread](sp-thread.md) и [spPCThread](sp-pc-thread.md): Create сохраняет
caller body parameter в thread `+18`, а OS routine получает полный thread.
Host Server может делегировать эти операции существующему thread-компоненту;
собственного Win32 backend в сетевой реконструкции нет.

Destructor проверяет IsRunning; при true записывает thread byte `1C=1`,
вызывает Wait с `1000` мс и **игнорирует его результат**, затем удаляет thread
через вторичный object interface `+4`. Далее выполняется Close, удаляются
принадлежащие Server peers, list container и lock storage, UDP/TCP streams,
закрывается и удаляется MemoryStream, освобождается список ID. Снятие
critical section перед освобождением lock storage не добавляется.

Реальный provider должен обеспечить отсутствие исполнения worker после
завершения lifetime Server. Native ожидание с проигнорированным timeout само
этого не гарантирует. Ограниченные последовательные fixtures не доказывают
thread scheduling, отсутствие races или корректность реального shutdown.

Close закрывает listening TCP только при ID `0`. Под peer lock он закрывает
TCP/UDP каждого peer, если его TCP network ещё подключён, и вызывает ReleaseId
для его ID независимо от состояния подключения. Затем закрывается UDP и
обнуляется `40`. Возвращается true даже при отказе закрытия сокетов; список
peers при этом сохраняется. Ошибки listening streams только логируются.

## Очередь и владение peer

SetQueue удаляет **предыдущий queue object**: освобождает lock, узлы списка,
sentinel и wrapper. Сами packet objects из удаляемых узлов не уничтожаются.
После этого сохраняет новый pointer и возвращает true. Destructor Server
финальный queue pointer не удаляет. Повтор того же pointer оригинал также
сначала освобождает; корректное происхождение и срок жизни являются
предпосылкой вызывающей стороны. Это нельзя автоматически считать безопасной
заменой произвольного embedded или borrowed контейнера менеджера.

[Host](../../../Sparkplug/Analysis/Host/spNetworkServerHost.h) передаёт
удаление внешнего queue representation отдельной обязательной операцией.
Он не подменяет восстановленный выбор момента удаления. Полноценный adapter
между этим владением и [NetworkManager](sp-network-manager.md) ещё требуется.

SetPeer дописывает pointer в список под lock и возвращает true, не меняя
`40`. Native destructor считает peer принадлежащим Server при ненулевом
byte `12`. Переносимый borrowed путь принимает только peers с нулевым byte;
для accepted peer требуется явная передача владения. Сам analytical API
не увеличивает `40`; это делает отдельный TCP admission после успешного Adopt.
Borrowed peer должен оставаться живым до уничтожения Server: Close и
RemovePeer не снимают его запись, а destructor повторно обращается к списку.

FindPeer при уже ненулевом выходном pointer возвращает true без проверки ID.
Иначе `BAD0` отклоняется, а под lock ищется последний peer с совпавшим ID и
ненулевым byte `12`. Borrowed peers при этом не выбираются. CountOwnedPeers
под тем же lock считает только ненулевые bytes `12`, независимо от connected.

PruneDisconnected для disconnected borrowed peer выставляет ошибку manager
`4` и запрашивает переход `(8,0)`, сохраняя запись. Accepted peer вызывает
ошибку `6`, удаляется вместе с записью, а ненулевой `40` уменьшается на один.
**Оригинальный обход после удаления пропускает следующую запись**; это
поведение сохранено. RemovePeer, напротив, освобождает ID и закрывает peer,
но сохраняет запись, объект и счётчик.

ReleaseId работает только при server ID `0`. Нулевой ID и ID не меньше
границы `16` лишь логируются. Допустимый ID добавляется в начало списка,
затем удаляются соседние дубликаты. Сортировки и глобальной уникальности нет.
AllocateId возвращает и удаляет первый uint16 списка, либо `BAD0`, когда
список пуст. Конструктор создаёт пустой список; из этого не следует правило
его первоначального заполнения перед запуском сессии.
Broadcast под lock отправляет пакет accepted peers, кроме совпавшего с
Packet.Source, через Peer SendPacket с аргументом `1`. Этот аргумент меняет
Packet.Destination; один packet последовательно изменяется при обходе.
Возвращается true независимо от результатов отправок.

## Отдельные управляющие пакеты

InitialInfoRequest принимает собственный peer receiver. Он читает состояние
текущего controller: допустимы `1` и `2`, причём проверка `2` делает новое
чтение под lock. Для другого состояния увеличивается statistic `21` и
возвращается false. Packet.Source `0` записывает Peer ID `0`; иначе Peer ID
`BAD0` заменяется результатом AllocateId. Повторный запрос сохраняет уже
присвоенный ID. Затем statistic `15` увеличивается перед отправкой type `1`,
payload из двух little-endian байтов ID, TCP, Source равен Server ID и
Destination `FFFF`. Положительный send result означает true; ноль и
отрицательное значение дают manager error `8` и false. Пустой ID pool
не отклоняется заранее: в ответ уходит `BAD0`.

InitialInfoReply читает uint16 из начала payload. Для `BAD0` сначала
запрашивает manager transition `(6,1)` и пишет diagnostic, затем **в любом
случае** сохраняет полученный ID и запрашивает `(5,0)`, возвращая true.
Приём packet header и проверка wire framing находятся за пределами этого
отдельного обработчика.

Ready сначала использует supplied peer либо FindPeer по Packet.Source.
Нулевой первый payload byte очищает Peer `+13` и возвращает true без проверки
диапазона ID и destination. Для ненулевого byte требуются unsigned
`Packet.Source <= CountOwnedPeers()` и `Packet.Destination == 0`: при успехе
Peer `+13=1`, иначе `+13=0` и false. Сравнивается число accepted peers,
а не граница `16` и не ID supplied peer. Это сохранённая асимметрия оригинала.
Данные правила не означают готовности игрового уровня или player actor.

HelloRequest аналогично разрешает peer. Первый timestamp записывается в
Peer `1C`; новое чтение времени записывается в payload word `+4`, остальные
байты сохраняются. Через общий [Peer](sp-network-peer.md) отправляются первые
8 байт, type `6`, TCP, Source равен server ID, Destination равен peer ID.
Положительный send result даёт true; ноль и отрицательное значение выставляют
manager error `8` и дают false. Эти ветви не увеличивают statistics. Timer
использует исходный протокол active → Start/Refresh → ticks/divisor.

Native обработчики напрямую читают payload без полной проверки размера.
Переносимый компонент требует, чтобы читаемый prefix был установлен в
[NetworkPacket](sp-network-packet.md); неизвестные байты и нулевой divisor
не заменяются выдуманными значениями.

## TCP admission и receive loop

Admission сначала сравнивает unsigned счётчик `40` с uint16 границей `16`.
При достижении границы возвращает false, сохраняя входной stream у caller;
Peer не создаётся. Иначе создаётся общий Peer и получает входной TCP stream,
а его UDP endpoint использует адрес этого TCP и младшие 16 бит **серверного
UDP stream** port word. Успешный Adopt дописывает owned Peer под lock и
увеличивает `40`; ID нового Peer пока остаётся `BAD0`.

Отказ UDP открытия после Adopt имеет необычное владение: original admission
возвращает false, но не удаляет созданный Peer или уже переданный TCP stream.
Обязательный Host sink сохраняет этот orphan owner явно, включая stream.
Исходный worker при false затем закрывает и удаляет исходный stream, что
оставило бы dangling ссылку у orphan Peer. Переносимый worker на таком
сочетании false и уже переданного stream останавливается исключением.
Эта unsafe ветвь не заявляется как эквивалентная завершённая операция worker.
Обычный capacity refusal сохраняет исходный Close → delete порядок caller.

Worker проверяет thread byte `1C` в начале каждого обхода. Он очищает UDP
sets `0`, затем `1`, затем `2`, прекращая цепочку при первом true. При Server
ID `0` выполняет TCP Accept и admission, после чего добавляет listening TCP
в sets `0/2`. Затем очищает общий MemoryStream, принимает UDP и передаёт
полученный stream в dispatcher с null peer, добавляя UDP в sets `0/2`.

Под peer lock worker читает время для каждой записи. Если unsigned разница
с timestamp больше `40000` и TCP connected, вызывается RemovePeer, а приём
для этой записи пропускается. Иначе общий buffer сбрасывается и получает
TCP данные через общий [SocketStream](sp-socket-stream.md). Отказ receive
пропускает запись; успешный нулевой receive вызывает RemovePeer. Ненулевой
stream передаётся dispatcher с данным peer. Всё ещё подключённый TCP
добавляется в sets `0/2`. После снятия lock выполняется PruneDisconnected,
затем UDP Select с `1000` мс; его результат игнорируется. Ровно `40000`
ещё не означает timeout. Nested lock при RemovePeer сохранён.

## Packet dispatcher и queue

Dispatcher делает Seek к началу и создаёт новый общий NetworkPacket для
каждого Read, включая последнюю неудачную попытку. Read false завершает
dispatcher с true; ошибка packet framing сама по себе не превращает этот
результат в false. Для каждого прочитанного packet вызывается внешний Dump
со свежим debug handle, даже null. Destination должен совпадать с текущим
Server ID либо быть `FFFF`; иначе пакет удаляется и чтение продолжается.

| Packet type | Действие после проверки destination |
| --- | --- |
| `0` | statistic `14`, InitialInfoRequest; требуется supplied peer |
| `1` | statistic `15`, InitialInfoReply |
| `3` | statistic `17`, Ready |
| `5` | statistic `19`, HelloRequest |
| `2`, `4`, прочие | Общая маршрутизация ниже |

Bool отдельного обработчика игнорируется, его packet затем удаляется и
чтение продолжается. Изменение Server ID обработчиком InitialInfoReply
применяется уже к следующему packet того же stream. Statistics обработчика
InitialInfoRequest и dispatcher являются отдельными действиями.

Общий пакет с destination `FFFF` при Server ID `0` проходит Broadcast,
после чего удаляется. Пакет с destination, равным Server ID, передаётся во
внешнюю queue с сохранением самого packet owner. Если queue не установлена,
native теряет этот pointer без удаления. Переносимый обязательный sink
`RetainUnqueuedPacket` сохраняет owner отдельно от доставленных пакетов и
за пределами Server lifetime. Клиентский общий broadcast в очередь не
попадает. Queue/sink являются внешними потребителями; собственного fallback
успеха для Server логики Host не содержит.

## Границы TCP между чтениями

Worker сбрасывает общий MemoryStream перед каждым приёмом. Он не сохраняет
неполный заголовок или payload для следующего прохода. Если один пакет
приходит двумя короткими блоками в разных проходах, каждый блок отдельно
попадает в Packet Read, завершается ошибкой и не образует общий packet.
Это проверено отдельно для разрыва внутри заголовка и внутри payload.
Ошибки обрезанного packet здесь рассматриваются при явном отказе stream
за границей данных; полный исходный error reporter этим не устанавливается.

Внутри одного ReceiveStream общий SocketStream, напротив, продолжает читать,
пока результат равен `500`. Две полные последовательные записи, пришедшие
как `500+14` или `500+0`, разбираются dispatcher в два packet. Нулевой
последний результат после полного блока сам по себе не уничтожает буфер.
Если после `500` приходит `-1`, ReceiveStream возвращает false: Worker
не вызывает dispatcher даже для полного первого packet в этих 500 байтах.
При следующем приёме Reset уничтожает старый prefix.

Восстановленный Worker сохраняет эти правила. Для надёжного TCP-сеанса
платформенный адаптер должен отдельно хранить незавершённые packet bytes
и различать неполные данные, EOF и ошибку. Такой адаптер является нашим
контрактом транспорта; исходному Worker он не приписывается.

## Оставшиеся границы

Первоначальное заполнение ID pool, producer границы `16`, присвоение
слушающему Server ID `0`, DGE, endpoint/configuration writers, реальные
потоки и game consumers ещё не закрыты. Admission debug содержит отдельный
глобальный log level и форматируемый diagnostic; его adapter обязан передать
фактическое значение, а не подставить успешный default. Debug-disabled
admission branches проверены оригинальными завершёнными вызовами. Отдельный
debug-enabled отказ adoption подтверждает через оригинальный resolver
фактический failure format, category и level `0`, а также сформированный
count diagnostic с level `1`. Debug-enabled capacity refusal этим
дополнительным вызовом не покрывается.
Проверки приёма используют actual SocketStream/MemoryStream/Packet и явные
Network.Receive результаты, включая несколько проходов Worker. Реальный
WinSock, scheduling, все возможные разбиения и рабочий transport adapter
этими ограниченными последовательностями не проверяются.

`SparkplugNetworkServerTests` проверяет завершённые операции, владение peers,
пропуск соседнего peer, Ready и Hello, повторное закрытие через destructor,
ошибочные результаты thread/socket boundaries и сохранение tail payload.
R3 добавляет ID issuance, запросы при разных controller states и send
результатах, все ветви packet switch, смешанные/обрезанные streams,
сохранение queued/orphan packets после Server destruction, TCP admission и
один bounded worker iteration с UDP/TCP/EOF/receive failure/timeout.
Отдельные framing cases проверяют сохранённые выше разбиения TCP, отказ
после полного блока, точные queued packets и срок жизни внешнего Peer.
Сравнение ограниченных оригинальных вызовов относится к их конкретным входам
и проекциям. Сборка, factory/RTTI или обмен через fixture не являются приёмкой
рабочего сервера или мода WinxCoop.
