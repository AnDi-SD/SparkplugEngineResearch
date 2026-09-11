# Winx Club + RTX Remix: подготовка интеграции

11 сентября 2026. Цель этого этапа — собрать сведения и подготовить проверяемый
путь подключения к `local-data/Winx Club`. Рабочая RTX-картинка пока не получена:
игра в этом исследовании не запускалась, изучены восстановленные пользователем
логи, файлы игры, существующее binary evidence и публичные исходники NVIDIA.

Рекомендуемый порядок: штатный runtime → проверка настроек и кадра → отдельная
x86 D3D9-прослойка при подтверждённой необходимости. Оснований переписывать
шейдеры игры или делать fork Remix сейчас нет. Прослойка — наш код совместимости;
она не меняет восстановленную игровую логику и не переносит D3D9 в приложения `tools/`.

## Что подтверждено на этой установке

| Проверка | Результат |
|---|---|
| Игра | `WinxClub.exe`, x86 PE32, 22 065 152 байта |
| SHA-256 EXE | `1B04DD9B082087141C840D6546B9B729FD1A3CD1BEBFABC1267035C2B30A4EC3` |
| Версия по логу | `remix-1.5.2+68edea01` |
| Официальный релиз | [1.5.2, 16 июня 2026](https://github.com/NVIDIAGameWorks/rtx-remix/releases/tag/remix-1.5.2), latest на момент проверки GitHub API |
| Runtime | 4/4 проверенных файлов совпали с официальными SHA-256: корневой `d3d9.dll`, `.trex/d3d9.dll`, `.trex/NvRemixBridge.exe`, `NvRemixLauncher32.exe` |
| GPU по сохранённому логу | RTX 3070, драйвер 610.88.0, Vulkan 1.4.341; устройство и swapchain созданы |
| Bridge | 32/64-bit handshake и создание серверного D3D9 device успешны |
| Игровой профиль | В логе отсутствует встроенный app config для `WinxClub.exe` |
| Рендерер EXE | 10/10 полных тел функций `PC_GRAPHICS_BODIES` совпали с существующим original evidence |

Хэши, параметры и адресные проверки записаны в
[машиночитаемом протоколе](../../research/winx-rtx-remix-2026-09-11.json).
Полный EXE отличается от pristine-эталона исследования. Совпадение десяти
функций позволяет использовать именно эти адресные сведения; оно не доказывает
тождественность всего EXE. Ничего в игре не исправлялось.

Сохранённый `remix-dxvk.log` от 10 сентября содержит:

```text
23:46:12.097 Trying to raytrace but not detecting a valid camera.
23:46:12.366 CameraManager: rejected an invalid camera
```

Есть также пропуск draw из-за отсутствующего texture hash, fallback sampler и
предупреждение о текстуре на mesh без UV. Они требуют сопоставления с конкретными
draw calls; ни одно не доказывает единственную причину чёрного экрана.
Bridge сообщает о завершении процесса клиента в конце записи — это не доказательство
сбоя инициализации RTX. Из логов неизвестно, дошёл ли пользователь до игрового мира.

Восстановленные конфиги и логи скопированы побайтно в локальный каталог
`local-data/rtx-remix/evidence/2026-09-11-restored/`. Он исключён из Git.

## Камера и шейдеры: что уже известно

По [исследованию камеры](native-class-sp-camera.md) игра публикует через D3D9:

| Состояние | Адрес Winx | Вызов |
|---|---|---|
| Projection | `0x004BBAE0` | `SetTransform(3)` |
| View | `0x004BBB20` | `SetTransform(2)` |
| World | `0x004BBB60` | `SetTransform(256)` |
| Viewport | `0x004BBA80` | `SetViewport` |

Эти функции вошли в сравнение с текущим EXE. Следовательно, Winx отличается
от игр, вообще не передающих fixed-function матрицы. Однако наличие функций
не доказывает правильное состояние матриц в момент проблемного draw.

[Обычная ветвь без весов](native-pc-renderer-fixed-draw.md) может рисовать с
нулевым vertex shader. [Персонажи](native-pc-skin-render.md) используют палитру
костей и генерируемые shaders. В локальном `Fixed.rfx` есть HLSL `vs_1_1`,
`BlendMatrices[16]`, пути для 0–4 весов, освещение и UV transforms;
`BasicShader.vsh` также `vs_1_1`. Название `Fixed.rfx` само по себе не означает
полное отсутствие программируемого vertex shader.

Remix уже умеет захватывать результаты простых vertex shaders; ему всё равно
нужны данные для восстановления 3D-сцены. Поддержка смешанного pipeline
ограничена, поэтому простое отключение всех shaders может разрушить skinning.
См. [официальное описание совместимости](https://docs.omniverse.nvidia.com/kit/docs/rtx_remix/latest/docs/introduction/intro-compatibility.html).

В [CameraManager релиза](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4/src/dxvk/rtx_render/rtx_camera_manager.cpp#L77)
сообщение об отклонении камеры возникает при `abs(shearX) > 0.01` либо
непрохождении проверки `fov >= 0.001` после разложения projection. Последняя
проверка отвергает и NaN. Сообщение выводится через `ONCE`: одна строка не
показывает число плохих кадров и не означает отказ всех камер.

Кандидаты для проверки: UI/заставка попала в выбор основной камеры;
необычная projection или её переход из perspective в 2D; устаревшее состояние
на конкретном draw; слишком ранняя вставка RTX относительно проходов мира.
Это гипотезы. У исходной камеры действительно есть stateful ветвь projection,
сохраняющая некоторые прежние cells, но её участие в этом сбое не установлено.

## Найденная ошибка настройки

В `rtx.conf`, строка 4:

```ini
rtx.useVertexCapture - True
```

[Парсер](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4/src/util/config/config.cpp#L988)
пропускает строку без `=`. Исправленный вариант для следующего опыта:

```ini
rtx.useVertexCapture = True
```

Но в [релизном коде](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4/src/d3d9/d3d9_rtx.h#L42)
значение по умолчанию уже `true`. Эту опечатку нельзя объявлять причиной сбоя.
Параметр `rtx.useWorldMatricesForShaders = False` действительно применился:
это видно в effective config. Он отключает использование world, но не
заменяет view/projection и не является универсальным исправлением камеры.
В этом этапе конфиг оставлен в исходном виде для воспроизводимости.

## Подключение GitHub как справочных исходников

Скачан самостоятельный checkout:

```text
local-data/rtx-remix/upstream/dxvk-remix/
```

Источник — [NVIDIAGameWorks/dxvk-remix](https://github.com/NVIDIAGameWorks/dxvk-remix).
Закреплён detached HEAD `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`:
именно на него ссылается gitlink `dxvk-remix` в официальном теге
`NVIDIAGameWorks/rtx-remix:remix-1.5.2`. Строка сборки `68edea01` отдельно
не разрешилась через публичный commits API; она не подменяется выдуманным SHA.

Исходники не изменены. `remote.origin.pushurl` установлен в
`DISABLED_READ_ONLY_REFERENCE` как защита от случайного push; это не файловая
ACL и не запрет намеренного изменения remote. Чтение и поиск доступны локально
без подключения нового аккаунта или плагина. Родительский проект игнорирует
`local-data/`, поэтому upstream и игровые файлы не попадут в его коммиты.

Submodules зависимостей не загружались: для чтения renderer, bridge и API
они сейчас не нужны. Этот checkout ещё не подготовленная среда сборки.
`rtx-remix` — сборочный/релизный верхний репозиторий; основной нужный нам код
находится в `dxvk-remix`, а 32-битный мост уже включён в его `bridge/`.
Toolkit на первом этапе совместимости не требуется.

## Предлагаемая прослойка

```text
WinxClub.exe (x86)
  → наш Winx D3D9 adapter (x86)
  → штатный Remix bridge d3d9.dll (x86)
  → NvRemixBridge.exe + .trex/d3d9.dll (x64)
  → Vulkan / RTX
```

Сначала adapter только записывает кадр, номер draw, viewport/render target,
world/view/projection, shader hash, declaration/FVF, диапазоны загружаемых
shader constants и depth state. Важны также `Reset`, state blocks и вызовы
`Draw*UP`; простой лог только `SetTransform` может пропустить восстановление
состояния. Запись ограничивается несколькими кадрами выбранной сцены.

После установления причины adapter сможет передавать согласованные матрицы
перед нужным draw и отделять UI/спецпроходы. Skinning и shader capture сначала
оставляем штатными. Для подключения предпочтителен отдельный модуль с hooks;
при DLL proxy нужно явно выстроить цепочку загрузки, так как имя `d3d9.dll`
уже занято мостом Remix. Нельзя подставлять 64-битную `.trex/d3d9.dll`
непосредственно в x86-процесс. Точная схема загрузки проверяется прототипом.

Внешние примеры: [xoxor4d/remix-comp-base](https://github.com/xoxor4d/remix-comp-base)
как основа compatibility-модов и
[RemixProjGroup/camera-proxy](https://github.com/RemixProjGroup/camera-proxy)
как пример передачи матриц перед draw. Это кандидаты для отдельного аудита,
не готовые проверенные решения для Winx. Универсальное угадывание матриц из
shader constants здесь менее привлекательно, чем известные функции Sparkplug.

**Ограничение Remix API:** полный SDK документирует `SetupCamera`, однако
в [32-битном bridge релиза](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4/bridge/src/client/remix_api.cpp#L416)
поле `interf.SetupCamera` не подключено; `Startup/Present` также отсутствуют
в выдаваемом интерфейсе. Частичный API требует `exposeRemixApi = True`.
Поэтому обещать «передадим камеру одним вызовом API из Winx» неправильно.
Для штатного x86 bridge основной путь камеры — D3D9 transforms.

## Первый законченный эксперимент

| Шаг | Что проверяет | Что сохранить |
|---|---|---|
| Исходная игра в выбранном игровом месте | Исправность сцены до Remix | Кадр, save/место и положение камеры |
| Remix с `rtx.enableRaytracing = False` | Работу bridge и обычной растеризации | Новый комплект логов и тот же кадр |
| RT включён, исправлен только синтаксис `useVertexCapture` | Базовое воспроизведение | Логи, effective config, кадры меню и мира отдельно |
| Один параметр за опыт: `useWorldMatricesForShaders` | Влияние поступающей world matrix | Сравнение мира и анимированного персонажа |
| Если отказ сохраняется — ограниченный trace adapter | Конкретную матрицу/проход, отвергаемый Remix | Числа матриц, shader hash, номер draw и frame |
| После появления сцены — свет, skinning, UI, переходы | Практическую пригодность | Движение камеры, анимацию, смену локации и capture сцены |

`rtx.fallbackLightMode = 2` полезен только как последующая проверка освещения,
когда геометрия и камера уже есть; свет не исправляет отсутствие камеры.
Готовность первого прототипа — управляемый 3D-мир и персонаж с правильными
трансформациями, а не одно открывшееся Remix overlay.

Для сборки нашей x86 DLL понадобятся MSVC C++ x86 и Windows SDK; зависимости
конкретной основы уточняются после её аудита. Пересборка всего runtime нужна
только при подтверждённой ошибке внутри Remix. Его
[релизный README](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4/README.md)
указывает Windows 10/11, VS 2019 (2022 не основной проверенный вариант),
Windows SDK 10.0.19041.0, Meson 1.8.2, Vulkan SDK >=1.4.313.2, Python >=3.9,
DirectX runtime и recursive dependencies. Полная сборка в этот этап не входит.

## Воспроизведение проверки

Наш [инспектор](../../research/inspect_winx_remix.py) читает файлы без запуска
игры и использования GPU. Для повторного запуска выбрать **новое** имя отчёта:

```powershell
python -B research/inspect_winx_remix.py --release-checksums local-data/rtx-remix/remix-1.5.2-release-crc.txt --output local-data/rtx-remix/inventory-next.json
```

Checksum-файл получен из
[официального release asset](https://github.com/NVIDIAGameWorks/rtx-remix/releases/download/remix-1.5.2/remix-1.5.2-release-crc.txt).
Скрипт использует существующие `read_pe`, `image_slice` и адресные эталоны
репозитория; новая игровая логика не создавалась. Проверка выполнена на выбранной
установке: 10 совпавших функций, 4 совпавших runtime-файла, одна строка с ошибкой
синтаксиса. Это проверка файлов, а не доказательство корректной живой камеры.
