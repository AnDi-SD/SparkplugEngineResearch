# Сеть PC: объекты и формат пакета

## Граница исследования

Девять concrete классов прошли factory, RTTI, Clone и два удаления: 45 операций. Factory базы `spNetwork` равна нулю. Ее getter, null Clone и вторичный объектный интерфейс подтверждены отдельно. Нулевой factory не объявляется признаком абстрактного C++ класса.

| Класс | Размер PC | Условия полного lifetime |
| --- | ---: | --- |
| spNetworkPacket | 40 | прежняя ограниченная heap/CloneManager обвязка |
| spNetworkManager | 460 | CRT и учет critical sections в одном потоке |
| spNetworkStateCtrl | 52 | те же CRT/synchronization inputs |
| spNetworkDebug | 100 | дополнительно timeGetTime = 1200 |
| spDXNetwork | 64 | выбранный отказ WinSock; object interface +4 |
| spNetworkPeer | 36 | выбранный отказ WinSock |
| spNetworkMatchmaking | 52 | выбранный отказ WinSock |
| spNetworkServer | 68 | выбранный отказ WinSock |
| spSocketStream | 48 | выбранный отказ WinSock |
| spNetwork | не измерен | только static interface/null Clone |

## Два интерфейса Network

У DXNetwork primary network table 006EF928 находится в начале объекта,
BaseObject/CrossPlatform table 006EF90C — по +4. Clone 004AD0C0 получает
вторичный this, создает complete object и возвращает новый object+4.
Регистрация пары использует secondary pointers. Delete adjustor 004AD050
возвращается к complete object. Snapshot/allocator учитывает весь 64-byte объект.

У базы destructor 004A1C00 записывает primary 006EEE50, сдвигает this на +4,
записывает secondary 006EEE30 и переходит к CrossPlatform destructor 00417AE0.
Secondary delete adjustor 004A1C30 вычитает четыре. Getter 004A1C20 возвращает
record 00762F30, Clone 004A1BF0 возвращает null.

## NetworkPacket и MemoryStream

Общая реализация [spNetworkPacket](../../reference/classes/sp-network-packet.md)
сохраняет раздельные scalar transfers, частично прочитанный заголовок,
закрытие stream при ошибке записи и неизвестные поля конструктора.
Подробный контракт и переносимые границы находятся в карточке класса.

Read 004996D0 и Write 00499850 используют заголовок из **17 байт**:

| Порядок | Поле | Ширина |
| ---: | --- | ---: |
| 1 | Source | uint16 |
| 2 | Destination | uint16 |
| 3 | PacketType | uint16 |
| 4 | UseTcp | uint8 |
| 5 | PacketDataField1 | uint32 |
| 6 | PacketDataField2 | uint32 |
| 7 | Size | uint16 |
| 8 | Data | Size байт |

Little endian, без выравнивания (`<HHHBIIH`). Названия полей подтверждают
оригинальные диагностические строки. Затем вызываются виртуальные
ReadData/WriteData потока. Clone 004999C0 создает свежие header и payload,
а не копирует содержимое исходного пакета.

Пять полных Read → Clone → Write опытов проверяют размеры 0/1/16/255/256,
включая raw TCP 255, точные wire bytes, positions, whole object/payload guards
и освобождение всех packet/stream allocations. Восемь усеченных заголовков
0/1/3/5/6/9/13/16: пустой поток действительно возвращает false; остальные
останавливаются перед следующим отсутствующим scalar read, сохраняя уже
измененные поля. Полная ветка логирования ошибки не подменяется и не считается
исполненной.

## Оценка и открытые части

Сокетные методы `spDXNetwork`, создание, Clone и освобождение восстановлены
как общий компонент с явным платформенным адаптером. Layout, операции,
общие наборы и особенности ошибок описаны в [карточке класса](../../reference/classes/sp-dx-network.md).
Базовый constructor, два интерфейса, null методы, копирование имени и
освобождение описаны отдельно для [spNetwork](../../reference/classes/sp-network.md).
Журнал, дамп пакетов, экранная диагностика и порядок освобождения
восстановлены для [spNetworkDebug](../../reference/classes/sp-network-debug.md).
Его переносимые операции используют явные часы, блокировку, поток и службы вывода.

[spNetworkManager](../../reference/classes/sp-network-manager.md) связывает server,
peer, matchmaking, controller, timer и debug. Восстановлены запуск, остановка,
ошибки, обработка очереди до 25 пакетов за вызов, probe/Hello, рассылка и ввод.
Особенности владения оставшимися пакетами при остановке сохранены и описаны в карточке.

[spNetworkMatchmaking](../../reference/classes/sp-network-matchmaking.md) владеет
memory stream и socket stream, обрабатывает входящие данные и синхронно рассылает
буфер уведомлением `0x22`. Нулевой успешный ответ закрывает соединение; при отказе
приёма закрывается только буфер. Жизненный цикл, порядок журнала и особенности
заимствованных данных описаны в карточке класса.

Открыты session/state protocols, buffers и queues с данными,
ошибки всех сетевых операций, thread interactions, полный packet error path,
размерные ограничения в callers и работа SocketStream с настоящим сокетом.

[spSocketStream](../../reference/classes/sp-socket-stream.md) содержит
открытие, приём и отправку сырых данных, принятие и замену сетевого объекта,
clone и удаление. Он сохраняет цикл приёма по 500 байт и успешные пустые
virtual операции. Его внешние сетевые зависимости подключаются через
обязательный host, общий memory stream не дублируется.
