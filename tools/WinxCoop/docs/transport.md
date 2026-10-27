# TCP transport без игры

`WindowsTcpTransport` — собственный последовательный adapter над Winsock.
Он использует `SessionCore`, `TcpPacketFramer` и общий Packet/MemoryStream
codec из Sparkplug. Формат и bootstrap описаны в
[контракте сессии](../../../docs/engine/architecture/network-session.md).
Это транспорт тестовых сообщений; он не запускает игру и не создаёт Блум.

## Владение и API

Один owner последовательно вызывает `Start`, `Pump`, `HostStart`,
`SendApplicationMessage` и забирает сообщения. Время — монотонные миллисекунды;
сетевых worker threads внутри нет. Метод `SendApplicationMessage` назван
явно, чтобы Windows macro `SendMessage` не менял имя C++-метода.

Host создаёт exclusive listener на заданном IPv4:port; port `0` назначает ОС.
Guest отдельно задаёт local bind и адрес Host. После connect/accept реальные
endpoints читаются из сокета и передаются новому SessionCore. Незавершённый
connect проверяется через `select`/`SO_ERROR`, имеет отдельный timeout.
В каждый момент разрешён один Guest; лишний сокет закрывается через RST.
Listener переживает закрытие участника и принимает новое соединение.

Каждое соединение получает новый возрастающий `connectionId` и новый Core.
Входящие сообщения сохраняют этот ID даже после разрыва. Отправитель обязан
передать ожидаемый ID; старая команда не попадёт новому участнику. Разрешённый
Host start token сохраняется на listener и применяется после Ready нового Guest.

Adapter удерживает frame и offset до завершения отправки. При WouldBlock
остаток остаётся на месте; очередной frame не обгоняет предыдущий. Системный
receive buffer ограничен 65536 байтами, очередь выдачи — 16 сообщениями.
При её заполнении чтение приостанавливается с сохранением buffer/cursor.
Положительный receive prefix обрабатывается до последующего EOF/error;
завершённые сообщения остаются доступными через `TakeMessage`.

Для приложения доступны семь из восьми outbound slots Core. При заполнении
`SendApplicationMessage` возвращает `false` без закрытия соединения; caller
сохраняет сообщение и повторяет попытку. Один slot оставлен control traffic.
Ошибка самого Core, timeout или нарушение wire-контракта завершает соединение.

`ShutdownSend` допустим после передачи всех очередей в системный send. Он
делает `SD_SEND`, запрещает новые application sends и heartbeat requests,
продолжает receive до EOF/error и ограничивает ожидание timeout. `ClosePeer`
закрывает участника; `Close` закрывает также listener, затем балансирует
WSAStartup/WSACleanup. Закрытый transport терминален. Для нового сервиса нужен
новый экземпляр. `LastTerminal` сохраняет причину, socket error и состояние Core.

## Диагностика и воспроизведение

Из корня репозитория:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxCoop/Build-NetworkChecks.ps1 -Configuration Release -BuildWorkers 1
python -X utf8 -B tools/WinxCoop/tests/test_transport_processes.py --exe artifacts/WinxCoop/network-checks/x64/Release/WinxCoopNetworkProbe.exe --output local-data/test-output/winxcoop-t-fresh
```

Каталог output должен быть новым. Harness запускает настоящие независимые
процессы и сохраняет их PID, endpoints, сообщения, события и владение ресурсами.
Для дополнительного соединения через адрес локального интерфейса передать
`--non-loopback <IPv4 этого компьютера>`. Это всё ещё одна ОС; другой компьютер
или виртуальная ОС проверяются отдельно. Настройки firewall автоматически
не меняются.

Сценарии проверяют ожидание разрешения Host, разрешение до подключения,
двусторонние payload 0/1/256 байт, реальные I/O с request cap в один байт,
переподключение, отказ лишнему Guest, недоступный порт и занятый bind,
освобождение endpoint, EOF/RST/timeout, неполные и неверные packets.
Отдельные случаи сохраняют готовый application payload перед EOF и перед
заголовком следующего packet с размером 257 байт.
Независимый Python fault peer посылает буквальные wire fixtures. Stress peer
удерживает чтение при малых socket buffers, затем проверяет FIFO и payload
4096 frames; успех требует настоящего `WSAEWOULDBLOCK` при send.

`WinxCoopNetworkProbe.exe` также можно запускать вручную:

```powershell
WinxCoopNetworkProbe.exe --role host --bind 127.0.0.1:27025 --preauthorize --echo-sizes 0,1,256 --run-ms 15000 --report host-fresh.json --ready-file host-ready-fresh.json
WinxCoopNetworkProbe.exe --role guest --bind 127.0.0.1:0 --server 127.0.0.1:27025 --echo-sizes 0,1,256 --exit-after-exchange --run-ms 10000 --report guest-fresh.json --ready-file guest-ready-fresh.json
```

Каждый JSON создаётся эксклюзивно, существующие файлы не перезаписываются.
Запись `sent` со `stage: queued` означает приём в очередь, доставку доказывает
отчёт другой стороны. `bytesSent/bytesReceived` — фактические результаты ОС.
`partialFrameAdvances` включает искусственно ограниченные requests;
`shortSendResults` считает только ответ ОС короче запрошенного размера.
Однобайтовый request сам по себе не доказывает native short send.

События ограничены кольцом 64; счётчик потерь должен оставаться нулевым в
приёмочном сценарии. В конце проверяются нулевое владение sockets, равенство
opened/closed и WSA starts/cleanups, отсутствие ошибок освобождения.
Проверка транспорта не доказывает LAN discovery, game-level readiness,
авторитет над уровнем, готовность графики VM или игрового прототипа.

Для отдельной XP VM доступен [независимый wire oracle](xp-socket-oracle.md).
Он проверяет совместимость и путь между ОС; production C++ binary для XP
этим не квалифицируется.
