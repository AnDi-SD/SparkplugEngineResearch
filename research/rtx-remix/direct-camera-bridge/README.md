# Прямая камера через x86 → x64 Remix bridge

Промежуточный проверяемый результат 13 сентября 2026. Собственный общий
transport patch поверх NVIDIA Remix bridge, без правил для игры/уровня/меша.
Обоснование и native camera ABI — в
[контракте камеры](../../../docs/research/winx-remix-native-camera-contract-2026-09-12.md).

**x86 client и x64 server собраны; production сериализатор проверен в обе
стороны. Отдельный live helper прошёл 20 camera → draw → Present кадров.
Пара ещё не установлена в игру; правильность игровых камер не проверена.**
Исходный `local-data/rtx-remix/upstream/dxvk-remix` остался чистым.

## Что изменено

`camera-v1.patch` применяется к точному base
`b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`. Редактируемая копия находится в
`local-data/rtx-remix/direct-camera-bridge-work`. Это reference v1.5.2;
исходник installed `68edea01` в локальном shallow repository отсутствует.

В публичный interface client подключён `SetupCamera`; единственная новая
command `371` добавлена в конец enum. Все 372 существующих имени/значения
(включая специальный `Bridge_Terminate=65535`) сохранены. Она отправляется
по существующей DeviceBridge queue, без новой очереди или нового Present.

Используется настоящий общий `bridge_util::Serializable<CameraInfo,true>`
в `util_remixapi.h/.cpp`. Wire packet содержит ровно 140 bytes:

| Смещение | Содержимое |
|---:|---|
| 0 | uint32 declared byteSize = 140 |
| 4 | uint32 CameraInfo sType |
| 8 | uint32 camera type: WORLD / SKY / VIEW_MODEL |
| 12 | 16 исходных float32 view, 64 bytes |
| 76 | 16 исходных float32 projection, 64 bytes |

Pointers и native padding не передаются. Client отклоняет null info,
неизвестные sType/type, non-null pNext и NaN/Inf до отправки. Parametrized
extension не поддерживается. Server сначала потребляет один полный data item,
проверяет оба размера, затем использует общий serializer; output остаётся
нетронутым при отказе. Поддерживается unaligned wire address.

Сервер вызывает свой `g_remix.SetupCamera` и **всегда** возвращает его error
code через UID-matched `Bridge_Response`, независимо от опции
`sendAllServerResponses`. Client ждёт этот response с существующим ack timeout.
Если server API отсутствует — NOT_INITIALIZED; malformed camera — INVALID_ARGUMENTS.
SUCCESS означает возврат actual renderer API, который сам ставит работу в CS queue;
это ещё не доказательство точных GPU camera values. Потеря transport response
после отправки — **terminal fault: client пишет UID/result и завершает весь
host process через TerminateProcess с кодом `0xE052CA01`**, затем abort как
защита от неожиданного возврата TerminateProcess. Это не raster fallback и
не восстановление соединения. Поздний ответ UID N иначе остаётся в голове
очереди и блокирует следующие sync-команды N+1. Обычный server API error по
корректному UID потребляется и возвращается без termination; local invalid
input отклоняется до отправки. Политика ограничена новой camera-командой.

Оба компонента получили version `remix-main-camera-v1+b81a7b56`. Существующий
server startup сравнивает version аргумент client до установления канала и
отклоняет смешанную пару. SDK interface/stock renderer не менялись. Это
проверка протокольной пары, а не доказательство полной runtime совместимости.

Два Meson изменения относятся к сборке: отдельное имя версии и возможность
собирать client/server вместе с test targets, чтобы повторно использовать util
library. Wrapper выбирает только x86 client / x64 server и camera test каждого
типа; launcher, остальные configurations и весь renderer не собираются.

## Проверка

Release сборки MSVC 19.51.36257, Windows SDK 10.0.26100.0, Meson 1.9.2,
Ninja 1.13.0, один worker. Компилятор отработал без выведенных предупреждений;
Meson выдаёт существующее предупреждение о будущем default `run_command(check)`.

Результаты `cross-arch-v3`: 214 checks у каждого writer, 219 у каждого reader,
**866/866**, return codes 0. Проверены WORLD/SKY/VIEW_MODEL, разные matrix
coefficients, отрицательный ноль/subnormal bit patterns, guards, unaligned
payload, все усечённые размеры, увеличенный размер, неверные sType/type,
unsupported pNext, NaN/Inf, отсутствие pointers и последующий корректный item
после malformed. Файлы из x86 и x64 одинаковы, 428 bytes,
SHA256 `9DF72EB1378A4E79EED00859B983DE037596444DFC4517D570D40CC6BF815AC2`.
Это файлы production payloads, не отдельный тестовый encoder и не live IPC.

Initial x86 initializer до добавления fail-stop проверен статически: RVA `5BEB0`, SetupCamera
назначается RVA `5B9F0` (interface offset `14`), в отличие от stock NULL.
Полный bounded disassembly сохранён с тестом; его SHA относится к initial
client из historical manifest, не к актуальной DLL. У актуальной DLL non-null
pointer подтверждён live helper. Применимость patch к чистому
reference и обратная проверка текущей копии прошли; reference не изменялся.
Prepare wrapper отдельно успешно проверил patch и hashes всех девяти файлов.

| Артефакт под `local-data/rtx-remix/direct-camera-bridge-build/` | SHA256 |
|---|---|
| `x86/src/client/d3d9.dll` | `79E88A694D233112605E7AB0F4F258F1FF536AD8471F67623C412DDAD6564F11` |
| `x64/src/server/NvRemixBridge.exe` | `C9D3E807CA36BFF6D3437D4DA3B8BB68DEED0EBEDDECC2E32B6E9D5547FD1927` |

Точные source/patch/binary/evidence hashes: [manifest.json](manifest.json).
Raw source digests описывают actual build inputs; `lfSha256` явно
нормализует CRLF для проверки будущего Windows checkout.
Большие артефакты/исходная копия в `local-data` в обычный clone не входят.

Сохранённые предварительные опыты: initial x86 build был tests-only и не имел
target d3d9; после поправки Meson обе сборки прошли. `cross-arch-v1` завершил
native writer успешно, но PowerShell потерял его ExitCode; wrapper теперь
удерживает process handle. `cross-arch-v2` прошёл четыре запуска, но записал
лишние PowerShell metadata в JSON. `v3` — итоговый полный clean report.
Эти прежние файлы не переписывались и не названы native failures.

## Изолированная live проверка

`local-data/rtx-remix/direct-camera-live/positive-v2` содержит исходник/EXE
helper, точные копии DLL, prepared/launch/result manifests и три журнала.
Stock `.trex/d3d9.dll` не менялся: SHA256
`F7C310821AA98BCDFDEC120330B0A89457B7C5EBA58D21464AF32639611C809F`.
Папка установки игры и read-only upstream не затрагивались.

Hidden owned окно 256×256, actual SetupCamera pointer non-null, SDK Present
pointer NULL. До CreateDevice valid camera дважды вернула server error 7;
между ними null/sType/pNext ошибки вернули 3 локально. После CreateDevice
20/20 camera вернули SUCCESS до каждого direct mesh draw, следующая config
команда и 20 обычных D3D Present прошли. API Startup/Present не вызываются,
D3D camera transforms и captured main draws в helper отсутствуют.
Возвращение из вызова не выдаётся за readback матрицы из renderer.

Exit 0, 22.404 с; camera mean **0.049590 мс**, p95 **0.105900 мс**, max
**0.201400 мс** на этом коротком прогоне. Это измерение sync IPC, без гарантии
такой же задержки в игре. Renderer дошёл до инициализации raytracing/NRC;
изображение и точное экранное движение не сверялись. Положительный trace
содержит 20 различных view translations и порядок camera перед draw/Present.

Каждый запуск создаётся suspended, проверяется полный image path, назначается
private job до ResumeThread. Лимит — 30 с, aggregate commit 4 GiB; cleanup
убивает только этот job и подтверждает пустой список PID. Фактический peak
positive-v2 — 2 762 874 880 bytes. Предварительный positive-v1 с 1 GiB завершился
на CreateDevice; renderer выбросил C++ exception. Причина недостаточного
лимита согласуется с последующим измерением, но тип исключения не установлен.
NVIDIA создаёт собственные nvngx_update/conhost children, поэтому список job
ограничен размером буфера, а не прежним package-smoke пределом 16 процессов.

Negative helper приостанавливает threads только bridge с проверенными
PID/path в том же job, затем посылает camera. Проверяется terminal exit
`0xE052CA01`, fatal log UID/result и cleanup всех owned descendants.
Текущий reproducer делает это до CreateDevice: GPU для потери IPC reply не
нужен. `fault-v1/v2` получили правильный exit после CreateDevice, но wrapper
слишком рано читал trace handle; итоговый wrapper ждёт закрытия job и до 2 с
доступности trace. `fault-v3` не дошёл до fault gate: stock device startup занял
около 20 с и превысил ожидание client. `fault-v4` остановился ещё на холодном
bridge handshake: renderer инициализировался после client timeout. Эти отчёты
сохранены, успешным end-to-end результатом текущего wrapper они не названы.

`Test-LiveEvidence.py` без запуска процессов независимо проверил сохранённые
positive-v2 и terminal fault-v1/v2 после cleanup: **3/3 native cases**. Сверены
hashes всех prepared inputs, path/job ownership, 20 результатов/Present/view
translations; для обоих faults — `0xE052CA01`, fatal UID=10/result=1, marker
перед вызовом и пустой job после cleanup. Результат
`closed-evidence-v1.json` не заменяет прежние failed wrapper reports. Нативный
отказ подтверждён дважды; новая fault-gate-before-device разновидность helper
и trace-retry на terminal exit пока не прошли end-to-end из-за startup timeout.

Начальные patch/manifest до terminal policy сохранены в `history/`; они не
описывают текущую DLL. После fail-stop сериализатор не менялся, поэтому его
866 checks повторно не запускались. Текущий patch проверяется независимо.

## Воспроизведение

Все команды выполнять из корня SparkplugEngineResearch. Reference должен
содержать указанный commit. Подготовка копии и pinned Detours:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/direct-camera-bridge/Prepare-Bridge.ps1 -InitializeDetours
```

Meson/Ninja установлены только в workspace, без изменения общей Python среды:

```powershell
python -m pip install --target local-data/rtx-remix/direct-camera-build-tools meson==1.9.2 ninja==1.13.0
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/direct-camera-bridge/Build-Bridge.ps1 -Platform x86 -Workers 1
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/direct-camera-bridge/Build-Bridge.ps1 -Platform x64 -Workers 1
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/direct-camera-bridge/Test-Camera.ps1
```

Штатный upstream build wrapper не выбирает установленный VS18; собственный
wrapper использует vswhere по компонентам x86/x64 и проверенный vcvarsall.
Для client потребовался ровно pinned Detours
`ea6c4ae7f3f1b1772b8a7cda4199230b932f5a50`; остальные renderer submodules
не загружались. Test script запускает скрытые helpers, каждый ограничен 30 с,
выходные каталоги должны быть новыми. Capture-Manifest.py фиксирует уже
существующий финальный v3 результат; не запускает сборку, игру или установку.

Для новых live каталогов при отсутствии других game/bridge процессов:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/direct-camera-bridge/Prepare-Live.ps1 -Name runtime-new
python -B research/rtx-remix/direct-camera-bridge/Run-Live.py --template runtime-new --name positive-new
python -B research/rtx-remix/direct-camera-bridge/Run-Live.py --template runtime-new --name fault-new --fault
```

Обычный режим скрытый; `--visible` допускает только принадлежащее helper
окно 256×256. Отсчёт 30 с начинается перед ResumeThread; копирование и сборка
в него не входят. Cleanup имеет отдельный максимум 2 с, чтение trace — 2 с.
Cold renderer startup может превысить лимит; это failure, не успешный camera test.

## Следующая проверка перед рабочим включением

Сохранить rollback обеих stock частей; устанавливать только согласованную
пару. Isolated capability/device/reply/error/Present проверены, но это не
доказательство полной совместимости reference с stock68edea01. Transport fault
завершит host; эту terminal policy необходимо учитывать до установки в игру.

Затем native camera packet после успешного `427D40`, первая WORLD camera до
первого captured main draw, D3D camera setters и один Present сохраняются.
Проверить accepted matrices и движение на Alfea/другом разрешённом уровне,
menu/UI/offscreen cameras, смену уровня и temporal history. Gardenia2 запрещён
пользователем. Этот patch не выбирает игровые камеры, не исправляет camera-cut
поведение renderer и не считается завершением этапа прямой камеры.
