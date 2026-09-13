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
1–50 PC и их доступность опубликованы в `WinxClubLevelCatalog`; слоты 38–40
пусты, а 50 (`Gardenia04`) неполон: PC-корпус содержит только SPL/SPT, PS2 — ни
одного файла triplet. Отдельно `Cloud02_06` является полным PC-уровнем, но
отсутствует в PS2 PCK. Значение `0`/`null` оставляет обычный startup.

Для обратной совместимости `NativeValidationRequest.Route` по умолчанию равен
`Contextual`. Пользовательские настройки Viewer отдельно выбирают `FastGeneric`
по умолчанию.

Изолированный запуск обязателен и использует `fullScreen=false`, отключает заставки,
звук, тени и exclusive mouse. Запрос с `UseIsolatedLaunchWorkspace=false`
отклоняется до создания процесса. Validator никогда не редактирует INI установки,
registry или игровые ресурсы: временная папка принадлежит сеансу, `Shaders`
копируются, а результаты `BuildAssetPath` внутри owned child process переводятся
на read-only `Media` рядом с явно выбранным executable. Это не позволяет registry
`MediaPath` незаметно смешать pristine executable с другой рабочей установкой.
Не переводимый к выбранному корню путь завершает проверку как `PathError`.
Явный `WorkingDirectory` нельзя смешивать с `UseIsolatedLaunchWorkspace`.

Штатный `FindMediaPath` выполняется полностью, включая чтение `MediaPath` и
`LanguageID`. Перед его последней null-проверкой validator проверяет поле manager
`+0x14`: если игра уже получила путь из registry, ничего не меняется; только при
нуле туда записывается выбранный корень `Media` из заранее выделенной памяти
owned child process, предотвращая штатный `PostQuitMessage`. Это one-shot
fallback для дочернего процесса, а не подмена функции или запись в HKLM. Обычный
запуск под нормальным пользовательским token использует нативный registry path;
ограниченные sandbox-token могут не пройти дальнейшую Direct3D-инициализацию и
не являются достоверным gameplay runtime.

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

Если `NativeValidationRequest.StringComparisonProbe` непустой, Core также
включает opt-in breakpoints для всех найденных `_stricmp`, `_strcmpi`,
`lstrcmpiA`, `strncmp`, MSVC `std::string::compare` и
`char_traits<char>::compare`. В журнал попадают comparator, операнды, call mode
и return address только для сравнений, где один операнд содержит фильтр без учёта
регистра. Особенно `char_traits` является горячей точкой: contextual run может
замедлиться примерно с 27 до 75 секунд и создать большой JSONL, поэтому probes
по умолчанию выключены.

Для exact `char_traits` events журнал дополнительно сохраняет EDI/EBP, безопасные
read-only snapshots `EDI+0x28/+0x2C`, `EBP+0x1C/+0x20` и несколько stack words.
Это сырые build-specific research locators: они помогают восстановить caller и
container value, но не участвуют в verdict и не объявляются общей ABI-схемой.

## Семантика результата

Подтверждённая последовательность:

```text
PathObserved -> PathRedirected -> ResourceLoad enter
-> FFPS header/magic/version checkpoints -> ResourceLoad return
-> Passed / EngineRejected / Crash / Timeout
```

Some level loads bypass `BuildAssetPath`: the engine calls `ResourceLoad` with
an already expanded absolute path under `Media`. The contextual route matches
that argument against the logical Media-relative suffix and redirects it at
`ResourceLoad` entry. This direct route emits the same `PathRedirected` event
and target-scoped FFPS evidence as the ordinary path-builder route.

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

`LogicalGameAssetPath` обязан включать игровой каталог, например
`Characters\Troll\Troll.smo`: совпадение только по имени файла неоднозначно и не
поддерживается.

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
