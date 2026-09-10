# Сеть PC: объекты и формат пакета

Блок цикла 10 сентября. [Контракт](../../research/network-family-contracts-2026-09-10.json)
связывает pristine PC SHA, таблицы, точные версии проверочных программ и все
неудачные/успешные runs. Исходные результаты находятся в
`local-data/results/native-cycle-20260910-1900/network-family/`.

## Граница исследования

Десять классов зарегистрированы в PC. В полном кэше PS2 соответствующих
регистраций нет. Это не доказательство отсутствия любого незарегистрированного
сетевого кода PS2; PC-оценки на PS2 не переносятся.

Девять concrete классов прошли factory, RTTI, Clone и два удаления: 45 операций.
Factory базы `spNetwork` равна нулю. Ее getter, null Clone и вторичный объектный
интерфейс подтверждены отдельно. Нулевой factory не объявляется признаком
абстрактного C++ класса.

| Класс | Размер PC | Factory | Object vtable | Условия полного lifetime |
|---|---:|---|---|---|
| spNetworkPacket | 40 | 00499960 | 006ED154 | прежняя ограниченная heap/CloneManager обвязка |
| spNetworkManager | 460 | 004512D0 | 006E6B3C | CRT и учет critical sections в одном потоке |
| spNetworkStateCtrl | 52 | 00482C70 | 006EBD3C | те же CRT/synchronization inputs |
| spNetworkDebug | 100 | 00481DF0 | 006EBBB8 | дополнительно timeGetTime = 1200 |
| spDXNetwork | 64 | 004AD060 | 006EF90C | выбранный отказ WinSock; object interface +4 |
| spNetworkPeer | 36 | 00482EB0 | 006EBD7C | выбранный отказ WinSock |
| spNetworkMatchmaking | 52 | 00482730 | 006EBCD0 | выбранный отказ WinSock |
| spNetworkServer | 68 | 00484290 | 006EC04C | выбранный отказ WinSock |
| spSocketStream | 48 | 00499410 | 006ED0A8 | выбранный отказ WinSock |
| spNetwork | не измерен | 0 | 006EEE30 | только static interface/null Clone |

Для последних пяти concrete lifetimes `WSAStartup` получает оригинальный
запрос версии 0202 и возвращает явно выбранное ненулевое 10091; WSADATA не
заполняется. `WSACleanup` возвращает FFFFFFFF. Никаких сетевых вызовов хоста
нет. Эти проверки не доказывают успешное соединение, отправку или прием.

CRT char_traits использует уже существующую общую fixture. Новый selector
подключает из прежней locale fixture только четыре synchronization imports:
Initialize/Delete/Enter/LeaveCriticalSection. Конкурентное исполнение не
проверялось. Для встроенного таймера NetworkDebug удалены прежние seams
Start/Stop, ограниченные глобальным timer receiver. Теперь исполняются
оригинальные Start/Stop, а literal ввод остается только на timeGetTime.

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

Первый DX pilot остановился на WSAStartup. Второй после введения явного отказа
ошибочно вызвал primary network method вместо RTTI; это ошибка исследовательской
обвязки, не null Clone игры. Третий использовал +4 и дошел до WSACleanup.
Четвертый с явным Cleanup input прошел все пять операций. Все промежуточные
исходники сохранены и проверены по source SHA. Игровые callbacks не подменялись
успехом. Окно с прежней scout-подписью `pc-network-base-construction` по 00465CE0
содержит protected путь DX constructor, вызывающий base 004A1C40; подпись не
принимается за доказательство типа. Линейный protected junk не считается кодом
исполненного пути.

## NetworkPacket и MemoryStream

Оригинальный Packet factory выделяет объект 40 и отдельный буфер 256 байт.
Source/Destination uint16 по +10/+12 получают BAD0. Data1/Data2 по +18/+1C и
Size uint16 по +20 обнуляются. PacketType +14 и UseTcp +16 сохраняют исходное
содержимое выделенной памяти; в fixture это CC. Padding +17 не сериализуется.
Payload pointer находится по +24. Нельзя заменять неинициализированные поля
предположением о нулевых игровых defaults.

Read 004996D0 и Write 00499850 используют заголовок из **17 байт**:

| Порядок | Поле | Ширина |
|---:|---|---:|
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

`probe_network_packet_io.py`: **15 случаев, 4,583 с**. Используются настоящие
MemoryStream factory 00465560 (56 байт, table 006E7E50), Resize 00465500,
Reset 00465550, scalar/raw IO и удаления. Заглушек stream callbacks нет.

Пять полных Read → Clone → Write опытов проверяют размеры 0/1/16/255/256,
включая raw TCP 255, точные wire bytes, positions, whole object/payload guards
и освобождение всех packet/stream allocations. Восемь усеченных заголовков
0/1/3/5/6/9/13/16: пустой поток действительно возвращает false; остальные
останавливаются перед следующим отсутствующим scalar read, сохраняя уже
измененные поля. Полная ветка логирования ошибки не подменяется и не считается
исполненной.

Два опыта Size=257 останавливаются перед оригинальным payload вызовом:
Read call 00499823 и Write call 0049992F. Аргументы уже содержат pointer на
256-byte allocation и request 257; stream position равна 17. Передача за
границы буфера не выполнялась. В самих этих методах внутреннего ограничения
256 перед transfer не найдено; внешние проверки сетевого вызывающего кода
еще не исследованы. Это не утверждение о доступной извне уязвимости.

## Оценка и открытые части

Первичные PC оценки: Packet 70; Manager/StateCtrl/Debug 25;
DXNetwork/Peer/Matchmaking/Server/SocketStream 20; Network 15.
Это оценка исследованных обязанностей класса, не доля инструкций и не
работоспособность сетевой игры. Новых полностью закрытых классов нет.

Открыты connection/session/state protocols, buffers и queues с данными,
ошибки всех сетевых операций, thread interactions, полный packet error path,
размерные ограничения в callers и работа SocketStream с настоящим сокетом.

