# WinxRemix

Экспериментальный адаптер PC-игры Winx Club к RTX Remix. Здесь находятся наш платформенный код, launchers, анализаторы и патчи внешнего bridge/renderer. Игровая логика по-прежнему берётся из общих `Sparkplug/` и `Winx/`.

## Запуск подготовленной установки

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Run-WinxRemix.ps1 -Mode RTX
```

Режимы `RTX`, `Raster` и `Original` выбирают трассировку Remix, его raster-путь и исходный D3D9. Launcher использует выбранную локальную копию игры и установленный прототип; сборка проверочной DLL сама по себе не подготавливает полный runtime. Одновременные запуски ограничивает скрипт.

## Зависимости и сборка

Версии внешних компонентов заданы в [dependencies.json](dependencies.json). Исходники Remix и игровые файлы хранятся локально, в Git входят только наши компоненты и патчи.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Prepare-Dependencies.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Build-Probe.ps1 -OutputDirectory artifacts/WinxRemix/build-check
```

Нужны Python 3, Git, MSVC C++ и Windows SDK. Подготовка получает недостающие внешние исходники; последующая сборка проверяет закреплённые зависимости.

## Границы прототипа

`-AutoSurfaceRoles` включает экспериментальную передачу совпадающих проходов через Remix API. **USD capture в этом режиме пока отключён от рабочего сценария: известен access violation в x64 Remix.** Скриншоты и диагностика адаптера имеют отдельный путь. Полная совместимость сцен, освещения и skin ещё не заявлена.

[Профили запуска, параметры и ограничения](docs/guide.md) · [Устройство адаптера](docs/development.md) · [Внешние компоненты](third-party/README.md).
