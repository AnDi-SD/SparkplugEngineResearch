# SmoNativeValidator.Core

Переиспользуемый Windows-модуль для проверки SMO нативным кодом
`WinxClub.exe`. Модуль не зависит от WPF и рассчитан на подключение как к
SmoViewer, так и к будущей версии SmoImporter.

## Распространение

Модуль является внутренней зависимостью приложений набора и распространяется
внутри их single-file сборок. Самостоятельный пользовательский пакет и отдельный
GitHub Release для `SmoNativeValidator` не формируются: Core, CLI и тесты остаются
в репозитории как исходный код для разработки и регрессионных прогонов.

## Что делает модуль

1. Находит executable автоматически либо принимает выбранный пользователем путь.
2. Разбирает PE32 x86 и находит внутренние функции загрузчика по masked-сигнатурам
   во всех executable sections независимо от SHA-256, размера файла и известных патчей.
3. Выбирает один из двух маршрутов: ранний универсальный smoke-test либо
   контекстную загрузку исходного игрового slot.
4. Копирует SMO и соседние `.stx`/`.spt` в короткий временный ASCII-путь под
   именем фактического trigger slot.
5. Перед каждым нативным запуском создаёт изолированный оконный workspace с частными
   `winx.ini`, `config.ini` и копией `Shaders`.
6. Запускает принадлежащий validator дочерний процесс игры под debugger.
7. Когда игра сама запрашивает trigger slot, перехватывает возврат
   `BuildAssetPath` и меняет путь только в памяти дочернего процесса.
8. Записывает checkpoints loader/FFPS, exceptions, timeout и результат в JSONL.

Игровой executable, штатный каталог `Media` и `HKLM\Software\Konami\Winx Club`
не изменяются. При остановке завершается только процесс, запущенный самим validator.

## API

```csharp
using SmoNativeValidator.Core;

WinxClubLocationResult location = WinxClubLocator.Locate();

NativeValidationReport report = await new WinxClubNativeValidator().ValidateAsync(
    new NativeValidationRequest
    {
        ExecutablePath = location.SelectedPath!,
        AssetPath = @"D:\work\Troll.smo",
        LogicalGameAssetPath = @"Characters\Troll\Troll.smo",
        Route = NativeValidationRoute.FastGeneric,
        UseIsolatedLaunchWorkspace = true
    },
    new Progress<NativeValidationEvent>(OnProgress),
    cancellationToken);
```

Ручной путь и общие настройки можно хранить через
`NativeValidatorSettingsStore`. По умолчанию полные журналы создаются в
`%LOCALAPPDATA%\SparkplugEngineResearch\SmoNativeValidator\Logs`.

## Маршруты запуска

`FastGeneric` подменяет гарантированно ранний запрос
`Menus\mousecursor.smo`. Это рекомендуемый быстрый smoke-test нативного loader:
он подтверждает `BuildAssetPath`, `ResourceLoad`, FFPS magic/version, создание
нативного ресурса и окно стабильности. Исходный `LogicalGameAssetPath` при этом
остаётся метаданными проверяемой модели, а `TriggerGameAssetPath` в отчёте явно
показывает реально перехваченный slot. Режим не доказывает работу модели в
конкретном gameplay-контексте, её скриптов и анимаций.

`Contextual` сохраняет прежнее поведение: игра должна сама запросить
`LogicalGameAssetPath`. Для быстрого перехода к нужной сцене изолированный запуск
может записать скрытый ключ `startLevel` в свой временный `winx.ini`. Значения
1–50 и их доступность опубликованы в `WinxClubLevelCatalog`; слоты 38–40 пусты,
а 50 (`Gardenia04`) неполон. Значение `0`/`null` оставляет обычный startup.

Для обратной совместимости `NativeValidationRequest.Route` по умолчанию равен
`Contextual`. Пользовательские настройки Viewer отдельно выбирают `FastGeneric`
по умолчанию.

Изолированный запуск обязателен и использует `fullScreen=false`, отключает заставки,
звук, тени и exclusive mouse. Запрос с `UseIsolatedLaunchWorkspace=false`
отклоняется до создания процесса. Validator никогда не редактирует INI установки: временная папка
принадлежит сеансу, `Shaders` копируются, а `Media` продолжает находиться самой
игрой через её штатный `MediaPath`. Явный `WorkingDirectory` нельзя смешивать с
`UseIsolatedLaunchWorkspace`.

## Распознавание executable

SHA-256 сохраняется только как диагностическое поле журнала и никогда не влияет
на разрешение запуска. Обязательные `BuildAssetPath`, `ResourceLoad` и цепочка
FFPS magic/version находятся сканированием внутреннего кода. Адреса сохраняются
как RVA и привязываются к фактическому image base процесса.

Если обязательная сигнатура отсутствует или неоднозначна, модуль возвращает
`InstrumentationUnavailable` с именем точки и кандидатами RVA: это ошибка
инструментирования, а не результат проверки модели. Необязательные Bloom-точки
при отсутствии пропускаются. Перед записью каждого `INT3` masked-сигнатура ещё
раз проверяется непосредственно в памяти дочернего процесса.

## Семантика результата

Подтверждённая последовательность:

```text
PathObserved -> PathRedirected -> ResourceLoad enter
-> FFPS header/magic/version checkpoints -> ResourceLoad return
-> Passed / EngineRejected / Crash / Timeout
```

`Passed` требует три target-scoped FFPS checkpoint, ненулевой результат
high-level loader и успешное окно наблюдения. Для `FastGeneric` это именно
native loader/scene-construction smoke-test; для `Contextual` дополнительно
сохраняется контекст естественного запроса ресурса. Ненулевой loader result без
подтверждённого checkpoint принятия версии возвращает `Inconclusive`, а не ложный
успех. Это не полный gameplay-тест и пока не per-object trace: адрес общего цикла
сериализации object directory ещё не восстановлен.

Игра должна естественно запросить указанный logical slot — прямой remote-call
loader намеренно не используется, поскольку его полный ABI/runtime context ещё
не подтверждены. Текущий MVP запускает игру сразу под debugger; некоторые
SecuROM-сборки могут отказаться от такого запуска. Late attach остаётся следующим
этапом исследования.

По умолчанию `LogicalGameAssetPath` обязан включать игровой каталог, например
`Characters\Troll\Troll.smo`: совпадение только по имени файла может подменить
одноимённый slot из другого каталога. Для заведомо безопасного специального случая
API оставляет явный `AllowFileNameOnlyLogicalPath = true`.

Software breakpoints переустанавливаются через single-step. Игра 2005 года в
исследованных путях практически однопоточна, однако одновременное прохождение той
же точки другим loader thread теоретически может пропустить checkpoint и дать
консервативный `Timeout`/`Inconclusive`. Приостановка остальных threads на окно
re-arm — следующий reliability-этап перед объявлением validator production-ready.

## Проверка

```powershell
dotnet run --project ../SmoNativeValidator.Tests/SmoNativeValidator.Tests.csproj
```

Тесты не запускают игру: они проверяют четыре доступных локальных executable,
независимость от SHA/размера overlay, отсутствующие и неоднозначные сигнатуры,
path matching, locator, tracker, JSONL/settings и Win32 interop layouts.
