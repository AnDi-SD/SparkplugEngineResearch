# SmoNativeValidator.Cli

Консольный запуск `SmoNativeValidator.Core` для одного SMO или последовательного набора моделей. CLI всегда запускает игру в изолированном оконном workspace с `fullScreen=false` и не меняет executable, registry или файлы установленной игры. Каждому кейсу назначается собственный JSONL-журнал, а после пакета создаются `summary.json` и `summary.tsv`.

CLI предназначен для разработки и регрессионных прогонов из исходников. Он не
входит отдельным EXE в пользовательский архив SmoViewer и не получает
самостоятельный GitHub Release.

CLI поддерживает два маршрута:

- `contextual` (по умолчанию, как и до появления маршрутов) ждёт запрос исходного
  `logical`-пути игрой. Это более медленная, но контекстная проверка модели;
- `fast` подменяет гарантированно ранний запрос `Menus\mousecursor.smo` и служит
  быстрым smoke-test нативного загрузчика, FFPS и создания ресурсов. Успешный
  `fast` не доказывает работу привязанных к исходному месту скриптов, анимаций
  или поведения уровня — для этого остаётся `contextual`.

Поле `logical` обязательно в обоих маршрутах: в `fast` оно сохраняет исходный
игровой путь модели как метаданные, тогда как реальный trigger отдельно
попадает в журнал и сводку.

## Один SMO

```powershell
dotnet run --project tools\SmoViewer\SmoNativeValidator.Cli -- `
  --exe X:\Games\WinxClub\WinxClub.exe `
  --asset X:\Models\Troll.smo `
  --logical Characters\Troll\Troll.smo `
  --output-dir X:\NativeRuns `
  --timeout 120
```

Имена со специальными для UI символами, включая `_`, передаются без ограничений. Если в пути есть пробелы, весь аргумент следует взять в кавычки.

Быстрый запуск в окне (`--isolated-windowed` оставлен как совместимый alias):

```powershell
dotnet run --project tools\SmoViewer\SmoNativeValidator.Cli -- `
  --exe X:\Games\WinxClub\WinxClub.exe `
  --asset X:\Models\Troll.smo `
  --logical Characters\Troll\Troll.smo `
  --route fast `
  --isolated-windowed `
  --timeout 20
```

Контекстную загрузку можно ускорить скрытым нативным `startLevel`:

```powershell
dotnet run --project tools\SmoViewer\SmoNativeValidator.Cli -- `
  --exe X:\Games\WinxClub\WinxClub.exe `
  --asset X:\Models\bloom_jeans.smo `
  --logical Characters\Bloom\bloom_jeans.smo `
  --route contextual `
  --isolated-windowed `
  --start-level 2
```

Для уровня, а не отдельного персонажа, можно потребовать явную готовность сцены:

```powershell
dotnet run --project tools\SmoViewer\SmoNativeValidator.Cli -- `
  --exe X:\Games\WinxClub\WinxClub.exe `
  --asset X:\Games\WinxClub\Media\Levels\Alfea\Alfea02.smo `
  --logical Levels\Alfea\Alfea02.smo `
  --route contextual `
  --start-level 28 `
  --require-scene-ready
```

В этом режиме одного ненулевого результата `ResourceLoad` недостаточно: validator
читает `wxGameFlowController` дочернего процесса и принимает сцену, только когда
текущий state и ID активного stack object совпали с запрошенным `startLevel`, а
pending state стал нулём. Поэтому `--require-scene-ready` требует положительный
`--start-level`. Буферизуемый игрой `GameStateLog.txt` не используется.

Изолированный оконный workspace создаётся по умолчанию; `--isolated-windowed`
можно не указывать. Он содержит принадлежащие сеансу INI и частную копию shaders,
а после проверки удаляется. Значение
`startLevel=0` означает обычный старт. Core проверяет таблицу игры и отвергает
значения `38`–`40` и `50`, у которых нет загружаемой SMO-сцены.

## Пакетный манифест

CLI принимает объект с массивом `cases` либо сам JSON-массив. Относительные `asset` и `workingDirectory` вычисляются относительно файла манифеста.

Gate 0-матрица двух уровней и персонажа находится в
[`manifests/mvp-gate0-scene-ready.json`](manifests/mvp-gate0-scene-ready.json).
Gate 3/5-матрица pristine → GLB/OBJ/FBX → pristine находится в
[`manifests/mvp-gate35-rigid-import.json`](manifests/mvp-gate35-rigid-import.json).
Финальная Gate 7-матрица уровней и skinned targets находится в
[`manifests/mvp-gate7-release.json`](manifests/mvp-gate7-release.json), а
одиночный Alfea gameplay-кейс — в
[`manifests/mvp-gate7-release-gameplay.json`](manifests/mvp-gate7-release-gameplay.json).
`scripts/Run-CollisionGameplayProbe.ps1` принимает этот или любой другой
manifest через необязательный `-ManifestPath`; без параметра сохраняется Gate 6
collision-сценарий по умолчанию.
Готовый tracked-набор из десяти Bloom-моделей находится в [`manifests/bloom-native-10.json`](manifests/bloom-native-10.json). Его asset-пути ведут в корневой `local-data` и вычисляются относительно самого манифеста. Все десять кейсов подменяют один слот `Characters\\Bloom\\bloom_jeans.smo`: заведомо рабочий и заведомо падающий UZHS-файлы, затем восемь pristine-моделей.

Запуск готового набора из корня репозитория:

```powershell
dotnet run --project tools\SmoViewer\SmoNativeValidator.Cli -- `
  --exe local-data\pc-pristine\WinxClub.exe `
  --manifest tools\SmoViewer\SmoNativeValidator.Cli\manifests\bloom-native-10.json `
  --output-dir local-data\native-validation-runs `
  --timeout 120
```

```json
{
  "cases": [
    {
      "name": "known-good Troll",
      "asset": "models/Troll.smo",
      "logical": "Characters\\Troll\\Troll.smo",
      "route": "fast",
      "useIsolatedLaunchWorkspace": true
    },
    {
      "name": "broken bloom jeans",
      "asset": "models/bloom_jeans_from_uzhs_crash.smo",
      "logical": "Characters\\Bloom\\bloom_jeans.smo",
      "route": "contextual",
      "startLevel": 2,
      "requireSceneReady": false,
      "useIsolatedLaunchWorkspace": true,
      "includeBloomCheckpoints": true,
      "timeoutSeconds": 120
    }
  ]
}
```

```powershell
dotnet run --project tools\SmoViewer\SmoNativeValidator.Cli -- `
  --exe X:\Games\WinxClub\WinxClub.exe `
  --manifest X:\NativeRuns\cases.json `
  --output-dir X:\NativeRuns
```

Кейсы выполняются строго последовательно. По умолчанию консоль показывает этапы
сеанса, целевые checkpoints и ошибки, а полный поток без фильтра сохраняется в
JSONL. Параметр `--verbose` дополнительно выводит все фоновые checkpoints и debug
output игры. Настройки в кейсе переопределяют параметры командной строки:

- `arguments`, `workingDirectory`;
- `route` (`fast`/`contextual`), `trigger`, `startLevel`,
  `requireSceneReady`, `useIsolatedLaunchWorkspace`
  (значение `false` отвергается до запуска игры);
- `timeoutSeconds`, `noProgressTimeoutSeconds`, `survivalWindowSeconds`;
- `includeBloomCheckpoints`, `collectFirstChanceExceptions`;
- `stringComparisonProbe` — opt-in фильтр выбранных native string comparators;
- `stageAsset`.

`trigger` — необязательное явное подтверждение ожидаемого пути маршрута: для
`fast` допустим `Menus\mousecursor.smo`, для `contextual` он должен совпасть с
`logical`. Обычно поле лучше не задавать, чтобы Core выбрал безопасное значение.
`startLevel` больше нуля допустим только для `contextual`; обязательный
изолированный workspace создаётся автоматически.
`requireSceneReady=true` также допустим только для `contextual` и сохраняет
результат в поле `sceneReadyReached` сводки.

CLI-эквивалент поля `stringComparisonProbe` — `--probe-string <text>`. Он
трассирует совпавшие `_stricmp`/`_strcmpi`/`lstrcmpiA`, `strncmp`, MSVC
`std::string::compare` и `char_traits<char>::compare`. Для PC-профиля тот же
opt-in включает `CP10` на записи `spTransformTrackEval + 0x10`: если стек lookup
соответствует фильтру, событие содержит evaluator, exact key, полученный
name/transform slot и предыдущее значение поля. `CP11` затем отслеживает до трёх
таких evaluator на входе и выходе transform-evaluation method и сохраняет
диагностические PRS tuples. Отсутствие `CP11` в конкретном маршруте не является
вердиктом об анимации — оно означает, что до конца окна не наблюдался активный
tick. Режим может увеличить contextual run с десятков секунд примерно до минуты
с лишним; без параметра comparator и оба transform checkpoints отключены.

Для каждого запуска внутри `--output-dir` создаётся каталог `run-<timestamp>`. Он содержит `<номер>_<имя>.jsonl`, `summary.json` и `summary.tsv`. Сводки сохраняют прежние поля и дополнительно записывают фактические `route`, `trigger`, применённый `startLevel`, `sceneReadyReached`, факт изолированного запуска, read-only `mediaSourceDirectory`, длительность отчёта Core, фазу падения и уверенность атрибуции. В консоль выводятся все события progress API. `Ctrl+C` передаёт отмену текущему validator-сеансу и прекращает пакет.

Полный список параметров:

```powershell
dotnet run --project tools\SmoViewer\SmoNativeValidator.Cli -- --help
```

Код возврата `0` означает, что все кейсы получили `Passed`; `1` — есть иной статус или ошибка runner; `2` — ошибка параметров/манифеста; `130` — отмена.
