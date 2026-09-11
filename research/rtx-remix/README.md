# Winx Club / RTX Remix: экспериментальная прослойка

Собственный платформенный код для выбранной копии `local-data/Winx Club`.
Это исследовательский прототип, не восстановленная игровая логика и не релиз.
Исходный Remix 1.5.2 и EXE игры остаются неизменными.

## Запуск установленного прототипа

В папке игры подготовлены `Play-RTX.cmd`, `Play-Original.cmd` и
`Play-Remix-Raster.cmd`. Они вызывают:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Run-WinxRemix.ps1 -Mode RTX
```

Режимы `RTX`, `Raster`, `Original` выбирают соответственно трассировку,
обычную отрисовку Remix и системный D3D9. Одновременные запуски запрещены
скриптом. Обычные профили не пишут подробную покадровую диагностику.
Каждый запуск сохраняет применённую конфигурацию и хеш DLL в новой папке
`local-data/rtx-remix/runs/play-*`.

Профили задают параметры через окружение дочернего процесса. Поэтому
для воспроизведения результата использовать эти команды, а не прямой
запуск WinxClub.exe. Игровые `rtx.conf`, `winx.ini`, `user.conf` не меняются.

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

`Original` выключает все эти адаптации. `Raster` включает всё, кроме UI boundary;
`RTX` включает все пять. FVF-нормализация и чтение текстур оставлены только
как выключенные по умолчанию диагностические эксперименты.

Проверены загрузка существующего сохранения Алфеи, RT-кадр с Блум,
изменение камеры, HUD и открытие дневника. Загрузочная картинка и дневник
проверены целиком при1280×960; уменьшение client до800×600 сохраняет весь кадр.
Остаются чрезмерно оранжевое освещение, отличия прозрачных/проекционных эффектов
и искажения фона меню.
Отдельные анимации/NPC, другие локации, переходы и длительная игра полностью
не проверены. Полноценный Remix-мод с заменами материалов и света не создан.

## Сборка и диагностика

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Build-Probe.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Start-Probe.ps1 -Name fresh-test -ExplicitMipLevels -ResubmitTextures -Raytracing -OrthographicUi -FitWindow -ViewportScale
```

Нужны MSVC x86 и Windows SDK. Сборка создаёт
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

Возврат штатного клиента Remix после выхода игры:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Restore-StockRemix.ps1
```

Скрипт проверяет SHA256 оригинала и сохраняет текущую DLL перед заменой.
Для обычной игры без Remix достаточно режима `Original`, перестановка DLL
для него не требуется.

## Upstream

Reference checkout: `local-data/rtx-remix/upstream/dxvk-remix`,
commit `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`, соответствующий Remix 1.5.2.
Его рабочее дерево чистое. Push URL отключён как защита от случайной отправки;
это не файловый read-only ACL. Сборка всего Remix для этих исправлений
не потребовалась.
