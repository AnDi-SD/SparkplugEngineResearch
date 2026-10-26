# WinxRemix: подробное руководство

## Запуск установленного прототипа

В папке игры подготовлены `Play-RTX.cmd`, `Play-RTX-Debug.cmd`, `Play-Original.cmd` и
`Play-Remix-Raster.cmd`. Они вызывают:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Run-WinxRemix.ps1 -Mode RTX
```

Режимы `RTX`, `Raster`, `Original` выбирают соответственно трассировку,
обычную отрисовку Remix и системный D3D9. Одновременные запуски запрещены
скриптом. Обычные профили не пишут подробную покадровую диагностику.
Каждый запуск сохраняет применённую конфигурацию и хеш DLL в новой папке
`local-data/rtx-remix/runs/play-*`.

Для отдельного подготовленного runtime можно явно выбрать
`-GameDirectory <каталог игры>` и `-ProbeDirectory <каталог сборки>`.
Оба каталога должны находиться внутри workspace; DLL в выбранной игре
сверяется с выбранной сборкой. Launcher выводит каталог игры и hashes
adapter/client/renderer/server; полный manifest находится в `launch.json`.
Это позволяет проверять новую сборку в отдельной копии игры.
Временный `bridge.conf` восстанавливается именно в выбранном каталоге,
если его содержимое не было независимо изменено после запуска.

`Play-RTX-Debug.cmd` передаёт `-DebugMenu` и запускает уже существующий
`WinxClubDebug.exe`, проверяя его хеш. F1 открывает штатное дебагменю;
LOAD LEVEL позволяет переходить между уровнями. Основной EXE не заменяется.

Профили задают параметры через окружение дочернего процесса. Поэтому
для воспроизведения результата использовать эти команды, а не прямой
запуск WinxClub.exe. Игровые `rtx.conf`, `winx.ini`, `user.conf` не меняются.

## Проверенные исправления

Экспериментальную [отправку Skin](skin-draw-source.md) включает `-SkinSubmit`
в RTX-профиле с `-DebugMenu` и `-SkinShaderContracts <каталог.wsf>`.
Она требует подготовленных расширений моста и renderer для палитры костей;
обычная поставка Remix сама по себе не подтверждает эту совместимость.
Автоматически включаются camera/light API и общие material channels.
Остальные настройки RTX-профиля сохраняются. Полная проверка игровой анимации,
переходов и истории движения остаётся открытой.

* `WINX_REMIX_EXPLICIT_MIPS=1`: явное корректное число mip-уровней при
  CreateTexture(Levels=0). Устраняет падение bridge на текстуре 3000×3000.
* `WINX_REMIX_RESUBMIT_TEXTURES=1`: повторная передача существующего
  содержимого managed 2D-текстур перед первым использованием. Восстановлены
  цвета и надписи в заставке и мире. Точный первоначальный путь потери
  текстур ещё требует исследования; это не объявлено исправлением игры.
* `WINX_REMIX_ORTHOGRAPHIC_UI=1`: обозначение начала fixed-function
  ортографического интерфейса невидимым треугольником нулевой площади.
  Настоящие UI-draws сохраняют Z-write; это необходимо для загрузочных
  картинок и порядка наложения фона. Основная поверхность запоминается
  при CreateDevice/Reset, изменённые состояния восстанавливаются.
* `WINX_REMIX_VIEWPORT_SCALE=1`: компенсация лишнего масштаба Remix после
  Reset. Runtime 1.5.2 умножает уже обновлённый viewport на отношение
  нового backbuffer к начальному. При 1024×768 →1280×960 это1,25.
  Компенсация действует только внутри draw, игровое состояние возвращается.
* `WINX_REMIX_FIT_WINDOW=1`: окно под размер кадра с ограничением рабочей
  областью монитора; весь backbuffer масштабируется в client area.
* `WINX_REMIX_MENU_BACKGROUND=1`: отдельная перспектива фона меню распознаётся
  по наблюдённой сигнатуре камеры и сохраняет штатную отрисовку. Убраны чёрные
  лучи в заставке, главном меню, выборе сохранений и дневнике.
* `WINX_REMIX_SKY_LAYERS=1`: ранние fixed-function слои вокруг камеры с
  отключённой глубиной помечаются как небо через временный MinZ=MaxZ=1.
  В Гардинии это устраняет ошибочное освещение от захваченного купола неба.
* `WINX_REMIX_SKIP_LEGACY_PROJECTED_SHADOWS=1`: исключает повторную проекцию
  старой 512×512 карты тени на землю. В Remix этот проход вызывает большие
  провалы поверхности. Основная геометрия остаётся, тени строит RT.
  Проверяются состояния прохода, тип/назначение текстуры и основная поверхность;
  нет зависимости от хэша текстуры, уровня или угла камеры.
* `WINX_REMIX_OPAQUE_ALPHA_TEST=1`: для непрозрачных fixed-function мировых
  draws выражает `alpha>=0` как `ALWAYS` и сразу восстанавливает состояние.
  Это одинаковый тест в игре; Remix получает явную непрозрачность основания.
  Вместе с категориями decal для травы `FAC245110A8BD959` и её края
  `3323174FD6FAE171` устраняет разрывы между совпадающими слоями Gardenia01.

`Original` выключает все эти адаптации. `Raster` включает mip, передачу текстур,
viewport и окно. `RTX` включает все девять. FVF-нормализация и чтение текстур оставлены только
как выключенные по умолчанию диагностические эксперименты.

RTX-профиль дополнительно сохраняет исходную яркость вершинных цветов
(`rtx.vertexColorIsBakedLighting=False`), задаёт интенсивности конвертации point/
directional света10, localtonemap exposure1 и shadows5. Это наш художественный
подбор для Алфеи и Гардинии, не восстановленные константы игры. Прежние настройки
без этого подбора остаются доступными через Start-Probe без ConfigOverride.

Проверены загрузка существующего сохранения Алфеи, RT-кадр с Блум,
изменение камеры, HUD и открытие дневника. Загрузочная картинка и дневник
проверены целиком при1280×960; уменьшение client до800×600 сохраняет весь кадр.
Проверен переход Алфея→Gardenia01 через F1. Цвета Алфеи приближены к исходным;
сняты сравнительные кадры. Большие провалы дорожки и склона в Gardenia01
устранены фильтром старой проецируемой тени; проверено переключение при
неподвижной камере и два разных ракурса. Разрывы по краям травы устраняет
совместная передача непрозрачного основания и decal-категорий верхних слоёв.
Остаются отличия освещения и других прозрачных эффектов.
Остальные уровни и длительная игра полностью не проверены. Полноценный Remix-мод
с индивидуальными заменами материалов и света не создан.

## Сборка и диагностика

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Build-Probe.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Start-Probe.ps1 -Name fresh-test -ExplicitMipLevels -ResubmitTextures -Raytracing -OrthographicUi -FitWindow -ViewportScale -MenuBackground -SkyLayers
```

Нужны MSVC x86, Windows SDK и `public/include/remix/remix_c.h` из reference
checkout ниже. Сборка создаёт
`local-data/rtx-remix/build/d3d9.dll`, сама её не устанавливает.
В текущей установке эта DLL уже лежит рядом с WinxClub.exe, а официальная
x86 DLL сохранена как `d3d9.remix-original.dll`.
Не копировать DLL поверх работающей игры.

`Start-Probe -LiveConfig` позволяет менять `rtx.*` через `live.conf` в папке
запуска и официальный SetConfigVariable. Это только диагностика, отключённая
в обычном профиле. Launcher временно включает API в bridge.conf, сохраняет
исходное состояние и восстанавливает его после выхода игры скрытым помощником.
Изменённый во время теста bridge.conf автоматически не перезаписывается.
Если подготовка или запуск завершается ошибкой до создания игрового процесса,
launcher синхронно откатывает подготовленный временный `bridge.conf` и сохраняет
исходную ошибку в `launch-failure.json`. Существовавший config восстанавливается
из собственных проверенных backup bytes; изначально отсутствовавший удаляется.
Изменённые target или backup отклоняются с сохранением файлов и исходной launch
error. Старый state без `beforeSha256` для существовавшего config также
отклоняется: у него нет подтверждённого содержимого backup.
Проверка target и запись последовательны и не дают атомарной блокировки от
другого writer; частичные IO failures не считаются успешным откатом. Ошибка
создания фонового restore worker после успешного создания игры остаётся
отдельной границей. Legacy surface-role mod этим config-откатом не возвращается.
Проверка без игры/GPU использует настоящий launcher с mocked process boundary:
`tests/runtime/Test-LaunchConfigRollback.ps1 -Name <fresh-name>`.
Из live.conf нельзя запускать код; удаление строки не сбрасывает ранее применённое
значение — для этого нужна явная обратная настройка или новый запуск.
Для сравнения теневого фильтра в запуске с `-SkipLegacyProjectedShadows`
разрешён собственный переключатель
`winx.keepLegacyProjectedShadows = True/False`. Он возвращает/убирает только
старый проход и не меняет камеру. Обычный профиль live API не включает.
Аналогично `winx.keepTrivialAlphaTest=True/False` в запуске с `-OpaqueAlphaTest`
возвращает исходную/эквивалентную запись альфа-теста.

`Start-Probe -StartLevel N -DebugMenu` создаёт отдельную рабочую папку с копией
INI и штатным `startLevel=N` (1–37, 41–49). Установленный INI сохраняется.
`Run-WinxRemix -Mode RTX -DebugMenu -StartLevel 27 -Windowed` открывает
Алфею с отдельным `fullScreen=false`, не меняя установленный INI.
Это запуск уровня, не возврат к произвольным координатам.
`-Windowed` требует `-StartLevel` для изоляции настройки.

Отдельная рабочая папка изолирует INI. Каталог игровых ресурсов и `Saved`
оригинал выбирает через [MediaPath](../../../docs/game/assets/resource-loading.md)
в 32-битном реестре Windows; копия EXE
рядом с другим `Media` сама по себе этот выбор не меняет. Launcher не меняет
реестр и не перенаправляет этот указатель в памяти игры.

`-LightWorldObserver` у `Run-WinxRemix.ps1` и `Start-Probe.ps1` включает
пассивное наблюдение исходных light world вызовов. Нужен проверенный `-DebugMenu`;
автоматически включаются `NativeUpdateSource` и `NativeCameraSource`.
Camera submit, `SceneLights` и Remix API этому наблюдателю не требуются:
допустимы `-Mode Original` и `-Backend system`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Run-WinxRemix.ps1 -Mode Original -DebugMenu -LightWorldObserver -StartLevel 4 -Windowed
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Start-Probe.ps1 -Name fresh-light-world -Backend system -DebugMenu -LightWorldObserver
```

В папке запуска создаётся `light-world-observer.jsonl` с лимитом 16 МиБ;
последняя ограниченная строка может пересечь границу. Журнал связывает
before/after bytes с завершёнными известными Root/Manager. В конце кадра
свежий loaded light registry для сцен с текущими tokens известного Root
даёт per-light status/type/enabled,
`worldPositionBits`, `cachedRangeBits` и counts. `Current` — совпадение post-call bytes при
текущих guards; `Changed` — изменение байтов; `NotObserved` — отсутствие
sample в этом scope; `Unqualified` — отказ квалификации. `NotObserved` не
доказывает пропуск обновления. Observer не исправляет dirty flags и не
подтверждает lifetime, свежесть всех producers или правильность RT-освещения.
Для наблюдавшихся обычных point lights за основной камерой подтверждены
вызов исходного world entry и совпадение их записей в полном реестре с post-call bytes
в текущем завершённом Root/Manager. Видимый receiver, область влияния света,
полнота всех producers, штатное завершение игры и результат Remix этим не
подтверждены. Устройство startup hook и CPU-проверки описаны в
[development](development.md). Полный игровой/GPU-сценарий и цикл Toolkit
для этого наблюдателя ещё не квалифицированы.

После завершения процесса анализатор сопоставляет журнал света с packet исходной
камеры и диагностикой кадра. Запускайте его для закрытой папки запуска и нового
выходного файла:

```powershell
python tools/WinxRemix/scripts/analysis/analyze_light_world_observer.py local-data/rtx-remix/runs/fresh-light-world --output artifacts/WinxRemix/fresh-light-world-analysis.json
```

Сопоставление требует одинаковых frame/scene, единственного camera packet и
стабильной диагностики этого кадра; ближайшая камера другого кадра не используется.
В зачёт наблюдавшегося включённого источника за камерой входят только enabled
`Current` point/spot. Cached position других observer statuses остаётся
отдельной диагностикой; directional/ambient не классифицируются по позиции.
Отсутствующая или неоднозначная камера, неактуальная диагностика кадра либо
нечисловые значения дают `Unqualified`.
В режиме Original/system `native=false`/`accepted=false` в camera packet допустимы:
исходные матрицы наблюдались, API submit не выполнялся.
Классификация использует cached world position и исходные row-vector матрицы
с численным допуском; она не доказывает влияние света или соответствие GPU.

`cachedRangeBits` содержит слово исходного DX cache без повторного вычисления.
Отдельная диагностика для связанного с камерой enabled `Current` point с
конечным положительным range проверяет, целиком ли cached сфера лежит за
плоскостью камеры, учитывая масштаб view. Пересечение и касание получают
отдельные результаты. Range-счётчики событий, пар frame/scene и адресных ключей
отделены от origin-счётчиков. Старый журнал без этого поля получает
`MissingCachedRange`, сохраняя анализ origin. Объём spot не квалифицирован;
cached сфера не доказывает RT-форму света, влияние на receiver, тени или
свежесть RT-данных.

Отчёт отдельно считает события, пары frame/scene и ключи адресов сцены.
Адресный ключ не является persistent ID или точным числом разных объектов.
Анализ сохраняет hash всех исходных байтов и автоматически отмечает
распознанный незавершённый JSON-префикс только в последнем хвосте;
завершённый JSON без последнего перевода строки учитывается. Повреждённые
завершённые или внутренние строки отклоняются. Поддерживается ограниченный
журнал 16 МиБ с последней строкой до 64 КиБ за границей.
Семантика и проверки анализатора описаны в
[development](development.md).

### Наблюдение исходной geometry selection

`-GeometrySnapshotObserver` у `Run-WinxRemix.ps1` и `Start-Probe.ps1` включает
отдельную пассивную диагностику перед расширением исходной selection.
Требуется `-DebugMenu`. Автоматически включаются mesh/camera/update sources
и журналы owner/transport; camera submit, Remix API и SceneGeometry extension
для этого режима не требуются. Допустимы Original/system:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Run-WinxRemix.ps1 -Mode Original -DebugMenu -GeometrySnapshotObserver -StartLevel 27 -Windowed
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Start-Probe.ps1 -Name fresh-geometry-observer -Backend system -DebugMenu -GeometrySnapshotObserver -StartLevel 27 -Windowed
```

На кадрах, кратных 300, observer заново читает loaded membership для текущей
selection независимо от scope cache и прежнего `SceneGeometryObserveOnly`.
`geometry-snapshot-observer.jsonl` содержит один объект scope с `rows[]`.
Зачёт требует вместе `completed`, `fencesStable` и `updateValid`, равных true.
Лимиты — 4096 rows, 4 МиБ на scope и 16 МиБ на журнал, с проверкой до записи.
Неполный scope не получает частичного зачёта.

World qualification и сравнение native CPU-covered VB/IB ranges с полными
uploaded buffers — отдельные признаки. Partial CPU capture сохраняет свои
флаги и не доказывает равенство всего буфера. Selected slots, supports,
registrations и model rows также считаются отдельно; это не число всех
объектов сцены, custom dynamic supports не покрыты. Числовые IDs действуют
только в текущем scope. Совпадение phase/world/mesh не подтверждает lifetime,
FreshRt, материал, draw eligibility или изображение Remix. Cached sphere
относительно камеры не доказывает положение всей реальной геометрии.
Observer не вызывает native producers или COM/API и не устанавливает Bytes
verified; собственный layout cache может заполняться. Устройство и CPU-стенд описаны в
[development](development.md).

### Проверка передачи шейдеров

`-ShaderAudit` у `Run-WinxRemix.ps1` или `Start-Probe.ps1` включает отдельный
аудит, работающий и при `-NoDrawTrace`. Обычные игровые профили его не включают.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Run-WinxRemix.ps1 -Mode RTX -DebugMenu -ShaderAudit
python -B tools/WinxRemix/scripts/analysis/analyze_shader_audit.py local-data/rtx-remix/runs/ИМЯ-ЗАПУСКА
```

Анализатор запускать после закрытия игры. Он требует установленную системную
`d3dx9_43.dll`; обёртка SDK находится рядом в `pc_shader_sdk.py`.
Анализатор компилирует отдельные файлы только в каталоге результатов,
не меняя игру. В `shader-coverage.json` сохраняются инвентарь `Shaders`,
результаты сопоставления и ограничения проверки.

В `shaders-client` находятся точные байткоды, возвращённые GetFunction, и
`audit.jsonl`: Create/SetShader, ошибки, счётчики всех шести типов SetConstants
и использование VS/PS на каждом из четырёх видов игровых Draw-вызовов.
GetShader перед draw учитывает также применение state blocks. Байткод
после Create сравнивается с исходным входом; игра получает исходный HRESULT.
Сводки сбрасываются раз в две секунды; F8 запрашивает дополнительную сводку.
Последние неполные две секунды перед закрытием могут не попасть в счётчики.
Созданные нашей обвязкой невидимые UI-треугольники не считаются игровыми draws.

Штатный `DXVK_SHADER_DUMP_PATH` направляет серверные `.dxso` и `.spv` в
`shaders-server`. Анализатор сравнивает клиентский и серверный байткод по SHA256.
Наличие соответствующего `.spv` подтверждает перевод программы, но не правильное
извлечение материала/геометрии для трассировки. Для `-Backend system` серверных
дампов ожидаемо нет. Ни один из режимов не доказывает покрытие непосещённых сцен.
Лимиты: 4096 уникальных программ, 16 МиБ байткода, 1 МиБ на программу;
превышение записывается в `readFailures`, а не выдаётся за полное покрытие.
Ex-only создание устройства не покрыто.
В системном Direct3D полнота Set/Constant-хуков не подтверждена; его
контрольный отчёт сравнивает байткоды и фактическое состояние при draw.

Возврат штатного клиента Remix после выхода игры:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/scripts/runtime/Restore-StockRemix.ps1
```

Скрипт проверяет SHA256 оригинала и сохраняет текущую DLL перед заменой.
Для обычной игры без Remix достаточно режима `Original`, перестановка DLL
для него не требуется.

## Материалы и ambient

`Run-WinxRemix.ps1 -MaterialAudit` или `Start-Probe.ps1 -MaterialAudit` включает
ограниченный `materials.jsonl`, независимо от DrawTrace/ShaderAudit/SceneLights.
Чтение происходит перед собственными преобразованиями draw. Сохраняются
RGBA diffuse/ambient/emissive/specular, источники цвета, флаги освещения,
восемь texture stages и выбранные именованные цветовые константы текущего VS.
Основной target, perspective/depth и наличие COLOR0/COLOR1/NORMAL/POSITIONT
позволяют разделять проходы; они не доказывают идентичность native камеры.
Материалы и shader constants этим режимом не изменяются.

`-ShaderSemantics` (только проверенный `-DebugMenu`) дополнительно связывает
оригинальные ключи shader manager с точным байткодом. Он автоматически включает
ShaderAudit и MaterialAudit. Пример универсального диагностического профиля:

```powershell
./Run-WinxRemix.ps1 -Mode RTX -DebugMenu -ShaderSemantics -SceneLights -SceneGeometry -StartLevel 27 -Windowed
```

После закрытия записи используйте общий анализатор байткодов из корня
репозитория. Он проверяет передачу программ; восстановление семантики ключей
шейдеров остаётся отдельной задачей.

```powershell
python tools/WinxRemix/scripts/analysis/analyze_shader_audit.py local-data/rtx-remix/runs/ИМЯ-ЗАПУСКА
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Run-WinxRemix.ps1 -Mode RTX -DebugMenu -SceneLights -MaterialAudit -StartLevel 4 -Windowed
python tools/WinxRemix/scripts/analysis/analyze_material_audit.py local-data/rtx-remix/runs/ИМЯ-ЗАПУСКА --verify-shaders
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/tests/materials/Test-MaterialAudit.ps1
```

Выборка: каждый 300-й кадр, начальная/Reset/F8 область той же диагностики.
Лимиты: 16 МиБ лога плюс последняя ограниченная запись, 1024 разных набора
входных данных, 256 shader tables и 1 МиБ их байткода. Счётчики failures/rejected
не позволяют принимать неполную выборку за полное покрытие. CTAB без известных
цветовых входов не доказывает отсутствие освещения в программе.
Тест сравнивает регистры 15 сохранённых игровых шейдеров с Microsoft D3DX
и отдельно проверяет 12 граничных случаев. Требуется локальный shader-audit corpus.
`Test-MaterialAudit.ps1`, `Test-SceneGeometry.ps1` и `Test-SurfaceMaterial.ps1`
принимают `-Name` для нового каталога результатов. Без него создаётся имя
с отметкой времени; существующий каталог повторно не используется.




## Явные FFP-материалы

`Run-WinxRemix.ps1 -Mode RTX -DebugMenu -SceneLights -SceneGeometry -MaterialChannels -LiveConfig -StartLevel 4 -Windowed`
подключает новый путь. В `run/live.conf` ключ `winx.keepMaterialChannels = True`
возвращает прежнюю отрисовку; `False` включает явные материалы снова.
Режим сохраняет RGBA/UV обычного неосвещённого FFP и разделяет представимые
коэффициенты lit FFP. Fixed.rfx ColorMode 4/5 и темноту он ещё не исправляет.

Для `LIGHTING=FALSE` разрешены вершины без нормалей: оригинальный FFP их не
использует. Адаптер строит плоскую нормаль каждого треугольника для RT,
сохраняя позиции, RGBA, UV и порядок обхода. Это наша политика освещения
трассируемой поверхности, а не восстановленная нормаль модели. Вырожденные
треугольники пропускаются; полностью вырожденная геометрия отклоняется.
Освещённый FFP по-прежнему требует исходные нормали.

## Upstream reference

Reference checkout: `local-data/rtx-remix/upstream/dxvk-remix`,
commit `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`, reference ветка Remix 1.5.2;
это не точный `remix-main+68edea01` установленного binary runtime.
Его рабочее дерево чистое. Push URL отключён как защита от случайной отправки;
это не файловый read-only ACL. Proxy собирается отдельно от renderer.
Экспериментальный Skin-путь дополнительно требует
[исправленных шагов массивов renderer](../patches/renderer-skinning-fix/README.md)
и расширений моста, описанных в руководстве отправки Skin.
