# Winx Club / RTX Remix: экспериментальная прослойка

Собственный платформенный код для выбранной копии `local-data/Winx Club`.
Это исследовательский прототип, не восстановленная игровая логика и не релиз.
Исходный Remix 1.5.2 и EXE игры остаются неизменными.

Универсальная ветка: `Run-WinxRemix.ps1 -Mode RTX -AutoSurfaceRoles`.
Она распознаёт совпадающие проходы по геометрии и состоянию, передаёт наложения
через `CreateMesh`/`DrawInstance`, не использует списки мешей/текстур и отключает
наш прежний USD-слой исключений. [План](../../docs/research/winx-remix-integration-plan.md)
и [проверка/ограничения](../../docs/research/winx-remix-auto-surface-roles-2026-09-12.md).

Пока это отдельный экспериментальный режим: **одна попытка штатного USD-захвата
с API-материалами завершилась access violation в x64 Remix**.
Не запускать capture в этом режиме до исправления;
скриншоты и аудит прослойки работают. Обычный профиль пока сохраняет предыдущий
вариант для сравнения. Его индивидуальные исключения не расширяются.

Автоматический режим требует `exposeRemixApi=True` и временно включает его
через launcher, восстанавливая bridge.conf после завершения игры. В каждом
run создаются `surface-roles.jsonl` и `surface-assets/` с точными локальными
DDS-копиями используемых текстур; они не входят в Git. Для запуска через
`Start-Probe` нужны `-AutoSurfaceRoles -OpaqueAlphaTest -Raytracing` вместе
с остальными проверенными адаптациями. `-LiveConfig` добавляет сравнение:
`winx.keepAutoSurfaceRoles=True` возвращает исходные draw calls,
`False` включает передачу наложений. Глобальные decal-категории при этом пусты.

## Запуск установленного прототипа

В папке игры подготовлены `Play-RTX.cmd`, `Play-RTX-Debug.cmd`, `Play-Original.cmd` и
`Play-Remix-Raster.cmd`. Они вызывают:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Run-WinxRemix.ps1 -Mode RTX
```

Режимы `RTX`, `Raster`, `Original` выбирают соответственно трассировку,
обычную отрисовку Remix и системный D3D9. Одновременные запуски запрещены
скриптом. Обычные профили не пишут подробную покадровую диагностику.
Каждый запуск сохраняет применённую конфигурацию и хеш DLL в новой папке
`local-data/rtx-remix/runs/play-*`.

`Play-RTX-Debug.cmd` передаёт `-DebugMenu` и запускает уже существующий
`WinxClubDebug.exe`, проверяя его хеш. F1 открывает штатное дебагменю;
LOAD LEVEL позволяет переходить между уровнями. Основной EXE не заменяется.

Профили задают параметры через окружение дочернего процесса. Поэтому
для воспроизведения результата использовать эти команды, а не прямой
запуск WinxClub.exe. Игровые `rtx.conf`, `winx.ini`, `user.conf` не меняются.

В установленной `.trex/bridge.conf` используется `clientChannelMemSize = 192MB`.
Шаблон — [bridge.conf](bridge.conf). На стандартных 96 МБ воспроизведён сбой
передачи текстур при загрузке Алфеи; увеличение буфера — проверяемый обход,
а не исправление внутреннего протокола Remix. Подробности и границы проверки:
[отчёт о вылете](../../docs/research/winx-remix-bridge-crash-2026-09-12.md).
Каждый Remix-запуск сохраняет также копию bridge.conf и её SHA256.

## Проверенные исправления

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

Общий контракт материалов для `-AutoSurfaceRoles` расширен: отдельные RGB/alpha
SELECTARG1/SELECTARG2/MODULATE, texture factor, выбранные UV и аффинный COUNT2.
Это передача FFP-наложений с выключенным освещением; albedo/ambient/emissive
ещё требуют раздельного переноса. [Проверки и границы](../../docs/research/winx-remix-surface-material-2026-09-12.md).
`powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Test-SurfaceMaterial.ps1`
проверяет RGBA и UV на скрытом system D3D9 render target; нужен доступ к GPU.
В `surface-roles.jsonl` событие `submit_material` показывает реально переданные
операции, texture factor, UV index и transform flags на sampled frames.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Build-Probe.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Start-Probe.ps1 -Name fresh-test -ExplicitMipLevels -ResubmitTextures -Raytracing -OrthographicUi -FitWindow -ViewportScale -MenuBackground -SkyLayers
```

Нужны MSVC x86, Windows SDK и `public/include/remix/remix_c.h` из reference
checkout ниже. Сборка создаёт
`local-data/rtx-remix/build/d3d9.dll`, сама её не устанавливает.
В текущей установке эта DLL уже лежит рядом с WinxClub.exe, а официальная
x86 DLL сохранена как `d3d9.remix-original.dll`.
Не копировать DLL поверх работающей игры.

`-Platform x64 -OutputDirectory local-data/rtx-remix/build-x64` нужен лишь
для наблюдения на стороне renderer; в итоговой установке x64-proxy убрана.
Ex-only CreateDeviceEx/PresentEx не покрыты. Диагностика состояния ограничена
выборкой; F8 возобновляет выборку x86. Подробности экспериментов и SHA256:
[живой отчёт](../../docs/research/winx-rtx-remix-live-2026-09-11.md).
Исправления окна и загрузочных экранов:
[проверка 11 сентября](../../docs/research/winx-remix-window-loading-2026-09-11.md).
Фон, цвета и небо: [сравнения и ограничения](../../docs/research/winx-remix-visuals-2026-09-11.md).

`Start-Probe -LiveConfig` позволяет менять `rtx.*` через `live.conf` в папке
запуска и официальный SetConfigVariable. Это только диагностика, отключённая
в обычном профиле. Launcher временно включает API в bridge.conf, сохраняет
исходное состояние и восстанавливает его после выхода игры скрытым помощником.
Изменённый во время теста bridge.conf автоматически не перезаписывается.
Из live.conf нельзя запускать код; удаление строки не сбрасывает ранее применённое
значение — для этого нужна явная обратная настройка или новый запуск.
Для сравнения теневого фильтра в запуске с `-SkipLegacyProjectedShadows`
разрешён собственный переключатель
`winx.keepLegacyProjectedShadows = True/False`. Он возвращает/убирает только
старый проход и не меняет камеру. Обычный профиль live API не включает.
Аналогично `winx.keepTrivialAlphaTest=True/False` в запуске с `-OpaqueAlphaTest`
возвращает исходную/эквивалентную запись альфа-теста.

Камера, причины провалов и проверка исправления:
[отчёт 12 сентября](../../docs/research/winx-remix-camera-ground-2026-09-12.md).

`Start-Probe -StartLevel N -DebugMenu` создаёт отдельную рабочую папку с копией
INI и штатным `startLevel=N` (1–37, 41–49). Установленный INI сохраняется.
`research/winx_remix_state.py --pid PID --cameras` читает состояние и камеры
из проверенного debug EXE; доступ к памяти только на чтение.
Дополнительный `--player` сохраняет мировые/локальные координаты Блум и
проверенную цепочку profile → player → node. Нулевая позиция во время
загрузки не означает готовность сцены. Телепорт этот инструмент не выполняет.

`--visibility` читает последний результат отбора и проверяет согласованность
чтения; его камера без синхронной записи неизвестна. `Start-Probe -SceneAudit
-DebugMenu` отдельно включает bounded detour original visibility для диагностики.
Обычные профили его не включают. [Контракт камеры](../../docs/research/winx-remix-camera-contract-2026-09-12.md).

`Run-WinxRemix -Mode RTX -DebugMenu -SceneLights -StartLevel 4 -Windowed`
включает общий экспериментальный профиль: AutoSurfaceRoles и источники из
Scene/LightManager через API, без индивидуальных asset-категорий. Направленные,
точечные и spot поддержаны; ambient/material lighting остаётся открытым.
Режим света работает без SceneAudit/draw logs и не меняет инструкции EXE.
В диагностическом LiveConfig `winx.keepSceneLights=True/False` переключает
legacy/scene путь. [Результаты и ограничения](../../docs/research/winx-remix-scene-lights-2026-09-12.md).
`Test-SceneLights.ps1` запускает47 проверок обвязки без игры/GPU.

`Run-WinxRemix -Mode RTX -DebugMenu -StartLevel 2 -Windowed` открывает
Гардинию 2 с отдельным `fullScreen=false`, не меняя установленный INI.
Это запуск уровня, не возврат к произвольным координатам.
`-Windowed` требует `-StartLevel` для изоляции настройки.

Обычный RTX-профиль также устанавливает собственный файл
`surface-roles/mod.usda` в `rtx-remix/mods/winx-surface-roles` рабочей папки.
Он задаёт категории двух исследованных поверхностей Гардинии 2 через
`preserveOriginalDrawCall`: геометрия, текстуры и трансформации остаются игровыми.
Это ограниченный каталог исправлений по mesh hash, не автоматическое
распознавание всех проходов. В диагностическом launcher он включается
отдельным `-SurfaceRoles`. Новый каталог mods обнаруживается при запуске;
в уже загруженном тесте можно сравнивать через `rtx.enableReplacementAssets`.
Подробности: [точка и классификация](../../docs/research/winx-remix-gardenia02-surface-roles-2026-09-12.md).

`research/winx_remix_sweep.py` — незавершённый тестовый водитель F1/загрузок,
не доказательство прохождения всех уровней. Обход остановлен по указанию
пользователя для исправления геометрии; помехи от tutorial/game-over сохраняются
как ошибки управления тестом, а не объявляются падениями уровней.

### Проверка передачи шейдеров

`-ShaderAudit` у `Run-WinxRemix.ps1` или `Start-Probe.ps1` включает отдельный
аудит, работающий и при `-NoDrawTrace`. Обычные игровые профили его не включают.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Run-WinxRemix.ps1 -Mode RTX -DebugMenu -ShaderAudit
python -B research/rtx-remix/analyze_shader_audit.py local-data/rtx-remix/runs/ИМЯ-ЗАПУСКА
```

Анализатор запускать после закрытия игры. Он требует установленную системную
`d3dx9_43.dll`; компилирует отдельные файлы только в каталоге результатов,
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

Результат проверки меню, загрузки, Алфеи, Гардинии и дневника:
[аудит 12 сентября](../../docs/research/winx-remix-shader-audit-2026-09-12.md).

Возврат штатного клиента Remix после выхода игры:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Restore-StockRemix.ps1
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
Точные байткоды новых VS сохраняются рядом как `materials.jsonl.shader-*.bin`;
это позволяет независимо проверить таблицы и повторно использовать их дальше.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Run-WinxRemix.ps1 -Mode RTX -DebugMenu -SceneLights -MaterialAudit -StartLevel 4 -Windowed
python research/rtx-remix/analyze_material_audit.py local-data/rtx-remix/runs/ИМЯ-ЗАПУСКА --verify-shaders
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Test-MaterialAudit.ps1
```

Выборка: каждый 300-й кадр, начальная/Reset/F8 область той же диагностики.
Лимиты: 16 МиБ лога плюс последняя ограниченная запись, 1024 разных набора
входных данных, 256 shader tables и 1 МиБ их байткода. Счётчики failures/rejected
не позволяют принимать неполную выборку за полное покрытие. CTAB без известных
цветовых входов не доказывает отсутствие освещения в программе.
Тест сравнивает регистры 15 сохранённых игровых шейдеров с Microsoft D3DX
и отдельно проверяет 12 граничных случаев. Требуется локальный shader-audit corpus.

[Результат и границы](../../docs/research/winx-remix-material-contract-2026-09-12.md).

## Сравнение света без перезапуска

Для запуска с `-SceneLights -LiveConfig` ключ `winx.sceneLightGain` в run/live.conf
меняет диагностический gain 0..1000 (обычно 10). Его изменения записываются в
scene-lights.jsonl, результаты generic RTX API-настроек — в live.conf.jsonl.
`research/winx_remix_compare.py --run ... --plan ... --name ...` снимает до 16
вариантов с проверкой native камеры и возвратом явной baseline из JSON-плана.
Схема и пример плана сохранены в [проверке Алфеи](../../docs/research/winx-remix-light-comparison-2026-09-12.md).

## Upstream reference

Reference checkout: `local-data/rtx-remix/upstream/dxvk-remix`,
commit `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`, reference ветка Remix 1.5.2;
это не точный `remix-main+68edea01` установленного binary runtime.
Его рабочее дерево чистое. Push URL отключён как защита от случайной отправки;
это не файловый read-only ACL. Сборка всего Remix для этих исправлений
не потребовалась.
