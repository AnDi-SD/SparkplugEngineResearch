# WinxGameControl

Средство проверки игрового ввода оригинальной PC Winx Club на Windows x64.
Управляет собственным виртуальным Xbox360-контроллером, отправляет конечные
scan-code/relative-mouse импульсы и отдельно наблюдает игровые кеши.
Это инструмент разработчика; для обычного запуска WinxCoop он не требуется.

Игровая память доступна только для чтения. Каждый сеанс выбирает полный
путь WinxClub.exe; перед действием проверяются PID, время создания, HWND,
сессия Windows и передний план. Активация окон остаётся за оператором.
Геймпад и SendInput глобальны для Windows: эти проверки ограничивают
отправитель, а не создают изолированное устройство для одного процесса.

## Сборка и зависимости

Нужны .NET8 на физическом ПК и уже установленный ViGEmBus. Программа
драйверы не устанавливает. Используется существующая managed SDK assembly
Nefarius.ViGEm.Client1.21.256.0; исходный default HintPath указан в проекте.
При другой установке передайте полный путь через `ViGEmClientAssembly`.

```powershell
dotnet build tools/WinxGameControl/WinxGameControl.csproj -c Release -m:1 -nr:false -p:UseSharedCompilation=false -o artifacts/WinxGameControl/Program
```

Результат — обычная framework-dependent программа Windows x64. Эта сборка
не заявляет поддержку Win7; нативные комплекты WinxCoop остаются отдельными.

## Сеанс

```text
WinxGameControl.exe serve <новая абсолютная папка> <секунды5..1800> --game <полный путь WinxClub.exe> [--no-pad]
```

Для фонового запуска используйте скрытое окно. Папка сеанса должна ещё не
существовать. По умолчанию `ready.json` означает создание собственного
контроллера и проверку точного нуля в его SDK-assigned XInput slot.
Начальный ненулевой пакет ViGEmBus устраняется до запуска выбранной игры:
LX=1 → проверка → все нули → проверка. См. исходные
[инициализацию и SubmitReportImpl драйвера](https://github.com/nefarius/ViGEmBus/blob/master/sys/XusbPdo.cpp).
Уже запущенный выбранный EXE при таком старте отвергается. Это не готовность игры.
При включении геймпада в игре сохраните INI, задайте `useGamePad=true` и
создайте устройство до запуска игры. Оконный режим — `fullScreen=false`.
Настройки читаются при старте; сам контроллер INI и сохранения не меняет.

Для обычной мыши/клавиатуры используйте `--no-pad`: он не создаёт устройство,
не требует работы ViGEmBus и не заявляет нейтральность чужих контроллеров.
В этом режиме `Pad` отсутствует в `ready.json`; `Neutral` — подтверждённая
граница сеанса без отправки отчёта геймпада. Одного `useGamePad=false`
оказалось недостаточно для mouse GUI при подключённом виртуальном pad:
проверяйте настоящий command18/GUI45 и наличие устройства при старте.

Выберите актуальное окно игры и активируйте его. Получите точную identity:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxGameControl/Get-Target.ps1 -GamePid <PID> -GamePath '<EXE>' -OutFile '<новый target.json>'
```

Используются нативный HWND и точный FILETIME. Не заменяйте их предположением
об opaque UI ID или округлённым JavaScript Number. Перевод EXE не отвергается
по полному хешу; readback отдельно проверяет нужные физические структуры.

## Команды

Запросы идут последовательно: `0001.json` → `0001-result.json` и далее.
`send_request.py` атомарно публикует один запрос, проверяет историю и ждёт
подтверждение. Не повторяйте действие при неизвестном результате.

```powershell
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action Bind --target '<target.json>'
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action Snapshot
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action Pad --ly 16000 --buttons X --hold-ms 150 --neutral-ms 200
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action Key --scan 0x1c --hold-ms 150 --neutral-ms 200
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action Mouse --dx 10 --dy 0
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action CenterSystemCursor
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action Neutral
python tools/WinxGameControl/send_request.py --session '<сеанс>' --action Stop
```

Числа времени в примерах описывают API, не универсальную приёмку игры.
Поддерживаются `Bind`, `Snapshot`, `Pad`, `Key`, `Mouse`, `CenterSystemCursor`, `Neutral`, `Stop`.
Оси стиков — signed16, триггеры —0..255; все оси и кнопки одного Pad отчёта
отправляются одновременно. Точные имена кнопок находятся в `ready.json`.
Mouse получает относительные counts, не экранную точку. `--button left`
или `right` добавляет Down/выдержку/Up; безопаснее сначала проверить hover
после отдельного движения. Keyboard получает базовый scan и отдельный
`--extended`, не engine key ID и не Unicode.

Если системный указатель вышел из client при относительном наведении,
`CenterSystemCursor` возвращает его в центр выбранного окна через SetCursorPos,
без button events. Перед кликом проверьте сохранность игрового ownCursor и hover.
В живой проверке это сохранило наведение, тогда как абсолютный CUA-click
однажды зажал игровой курсор у0,0. Screen point и положение игрового курсора
нельзя взаимозаменять. Нажатия отправляются только при capture игры либо
проверенном системном указателе внутри её unobscured client.

Перед сменой окна выполните Neutral в старом активном окне и получите
подтверждение; затем активируйте новое окно и Bind его свежую identity.
Контроллер при этом не пересоздаётся. Receipt имеет конечный срок и
аннулируется любым следующим Pad/Key/Mouse действием. Нулевой OS отчёт
отделён от нулевого кеша Winx; свежесть DirectInput Poll и native GUID
не подменяются предположением о SDK устройстве.

## Проверка результата

`events.jsonl`, результаты каждого запроса и `final.json` сохраняются
локально. Действия содержат before/during/after: API, XInput, Winx caches,
курсор/GUI, таймер, flow и позу персонажа, когда эти структуры доступны.
`successful` означает завершение ограниченного действия без ошибки API/
guard; принятие игрой подтверждается её кешем и видимым поведением.
`qualified=false/reason` не является нулевым игровым состоянием.

Короткий Down+Up может схлопнуться в одном DirectInput опросе. Мышь Winx
имеет собственный курсор, GUI в pad-режиме может быть выключен. Модель этих
путей описана в [cached input](../../docs/engine/architecture/input-cached-state.md)
и [spDXMouse](../../docs/reference/classes/sp-dx-mouse.md).

При завершении проверяйте exact-zero, отключение собственного контроллера,
его исчезновение и `cleanup` в final. Успех `ResetReport`, наличие устройства
или один OS readback не заменяют эту проверку. Сохранённые настройки
восстанавливаются только при закрытой игре и без перезаписи чужих изменений.
Штатный Quit может завершить игру до окончания neutral interval действия:
такой запрос сохраняет ошибку lifetime, а отдельный `final.reason=game-exited`
фиксирует естественное завершение процесса и cleanup; это не авария игры.
