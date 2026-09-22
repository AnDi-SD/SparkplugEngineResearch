# Mesh skinning bridge

Our x86-to-x64 Remix transport. Build inputs and patch hashes: [manifest.json](manifest.json).

The API's `blendWeights_count` and `blendIndices_count` are total array element
counts, normally `vertexCount*bonesPerVertex`. The previous bridge multiplied
these counts by both factors again. A three-vertex mesh with two influences and
six floats therefore read 144 bytes from a valid 24-byte weight array. The same
problem affected indices. The corrected wire copies exactly `count*4` bytes.
Rigid mesh wire is unchanged. Vertex padding is omitted: one vertex is 36 wire
bytes, not the 64-byte public structure. Bone transforms remain 48-byte affine
matrices and pass through the existing Instance extension chain.

Both sides must be updated together. The bridge version is
`remix-main-skinwire-v1`; the existing server startup comparison rejects a mixed
old/new pair. The full patch includes the previously qualified camera command
and server instance audit. The incremental patch contains this task alone and
requires the exact `before-v1` source snapshot. The immutable reference remains
at `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4` and is only read or checked.

Mesh preflight checks required pointers, count equality and 32-bit wire size
arithmetic before opening a command. Unsupported Mesh `pNext` is rejected;
previously that branch could loop indefinitely. Palette count is limited to 256,
with a required pointer when nonempty. DrawInstance preflight bounds its chain
to 64 extensions and rejects duplicate bone palettes. Input pointers must remain
readable and stable throughout the call. Weight values, normalization and index
values are renderer/input semantics and are not silently rewritten here.

The server validates the entire mesh or palette payload before invoking the
existing owning decoder. Invalid mesh payloads consume their following handle
UID and skip creation. Invalid or duplicate palette payloads are drained with
the Instance extension chain and skip its renderer call. Other commands retain
their existing behavior. This is not a general hostile-IPC or OOM recovery layer.

Owning decode uses typed arrays with matching `delete[]`, releases the surfaces
array itself, and initializes all partial ownership before further allocation.
The client serializer temporary also uses its matching array delete. These
changes preserve the original caller ownership of serialized inputs.


## Build and tests

Use `Test-Skinning.ps1`, `Build-Pair.ps1` and the camera regression in the adjacent component. `Capture-Manifest.py` stores a fresh detailed report in the ignored private evidence directory; it does not replace the public build manifest. CPU checks do not establish GPU deformation or compatibility with every game resource.

`Test-BakedPackets.ps1 -Name fresh-name -Manifest paths.txt` exercises the shared
[baked Skin consumer](../docs/skin-packets.md) with captured packets. It builds
x86/x64 fixtures from a frozen source snapshot, routes actual compact Mesh,
identity Instance and Blend arguments through `util_remixapi.cpp`, and reads
each architecture's file on the other. All meaningful vertex/index/instance
fields are compared exactly; absent skinning stays absent. Mesh ownership is
released before its material. The bounded file exchange is not bridge IPC or
a renderer run. Existing bridge working sources must already contain the
qualified patch; the test records their hashes and never edits those sources.

## Signed-пакеты общего адаптера

`Test-BakedPackets.ps1 -Name <новый запуск> -Manifest <список SKP1> -SignedWeights`
проверяет постоянную исходную геометрию с адаптированными весами и две палитры
на каждый пакет. Настоящие serializer и owning decoder обмениваются файлами
между x86 и x64 в обе стороны; сравниваются все веса, индексы и матрицы.
Этот вариант дополнительно проверяет BoneTransforms и отказ на обрезанном
payload. Он не запускает IPC, GPU или игру. Без `-SignedWeights` сохраняется
проверка общего CPU-baked потребителя.

## Ожидание остановки сервера

Подтверждение `Bridge_Ack` при остановке означает, что сервер обработал команду.
Процесс ещё может завершать работу DLL и системных ресурсов. Исходный клиент
ждёт его только три секунды, затем вызывает `TerminateProcess` с кодом 1.
Сообщение сервера об успешной очистке может появиться до этого принудительного
завершения.

Наш дополнительный [child-shutdown-v1.patch](child-shutdown-v1.patch) увеличивает
ожидание до десяти секунд. Код завершения сервера, принудительное завершение по
таймауту и формат обмена сохраняются. Патч применяется к свежей рабочей копии
закреплённого upstream; его можно добавить после полного skinning-wire патча.
Контрольные суммы и параметры находятся в [shutdown-manifest.json](shutdown-manifest.json).
MIT-уведомление исходного файла сохраняется.

```powershell
git -C <fresh-source> -c core.autocrlf=false apply --check <absolute-path-to-child-shutdown-v1.patch>
git -C <fresh-source> -c core.autocrlf=false apply <absolute-path-to-child-shutdown-v1.patch>
```

Хеши исходника в manifest рассчитываются после нормализации CRLF в LF.
Собирайте новую пару в отдельных каталогах x86 и x64 через `meson setup` и
`meson compile -j 1`, в окружении соответствующего MSVC. Сохраняйте одинаковую
версию транспорта на обеих сторонах. Уже выполненный опыт и его исходники не
используются для повторной сборки.

Для отдельной проверки ожидания:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/skinning-bridge/Test-ChildShutdown.ps1 -Name fresh-shutdown-test -SourceDirectory <unmodified-pinned-source>
```

Тест создаёт свежий каталог, извлекает точное тело upstream-метода и сравнивает
его с вариантом ожидания десять секунд. Собственные дочерние процессы завершаются
быстро, через четыре секунды, с заданной ошибкой либо позже предела ожидания.
Удерживаемый дескриптор позволяет проверить фактический код выхода после закрытия
дескриптора внутри метода. Launcher контролирует память, время и оставшиеся
собственные процессы; `result.json` появляется после проверки результатов.

Это тест метода ожидания. Он не проверяет регистрацию callback, владение
дескриптором клиента в сервере или освобождение внутренних объектов renderer.
Для полной пары требуется отдельный [графический тест](../docs/skinning-fixture.md)
с проверкой кода выхода обоих процессов. Задержка до десяти секунд не исправляет
зависание renderer и не гарантирует успешную остановку игры.

## Дескриптор клиента в сервере

`GetCurrentProcessHandle()` создаёт через `DuplicateHandle` дескриптор процесса
клиента в таблице дескрипторов сервера и отправляет его в `Bridge_Syn`. Числовое
значение имеет смысл в процессе сервера. В клиенте такое же значение может
обозначать другое событие, файл или объект синхронизации.

Исходный `releaseChildProcess()` вызывает `CloseHandle(hDuplicate)` в клиенте.
Наш [remote-handle-v1.patch](remote-handle-v1.patch) удаляет этот вызов: таблица
сервера освобождается при выходе его процесса. Локальный дескриптор самого
дочернего процесса по-прежнему закрывается после ожидания. Порядок callback,
формат обмена и код renderer сохраняются.

Патч применяется после `child-shutdown-v1.patch` в свежей копии исходников;
его параметры описаны в [remote-handle-manifest.json](remote-handle-manifest.json).
Проверка и сборка используют тот же порядок `git apply --check`, `git apply`
и отдельные каталоги Meson, что и патч ожидания.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/skinning-bridge/Test-ChildShutdown.ps1 -Name fresh-remote-handle-test -Kind RemoteHandle -SourceDirectory <unmodified-pinned-source>
```

При подготовке стенда режим `RemoteHandle` применяет к извлечённому тексту
проверенное изменение ожидания и сверяет хеши обоих тел метода. Стенд создаёт
события в собственном родительском процессе и дублирует дескриптор родителя в
собственный дочерний процесс до совпадения числовых значений. Обратное
дублирование подтверждает, что серверный дескриптор обозначает процесс,
а локальный — другое событие. Исходный метод закрывает это событие; исправленный
сохраняет его пригодным к использованию. Проверяются фактический выход дочернего
процесса и возврат числа локальных дескрипторов к исходному после каждого случая.
Первое создание процесса измеряется отдельно, чтобы его инициализация не
попадала в баланс проверяемого метода. Регистрация callback остаётся за пределами
этого стенда; применение исправления не доказывает причину любой ошибки выхода игры.

## Материалы Subsurface

[Передача материала Subsurface](subsurface-test/README.md) описывает дополнение
пяти пропущенных полей, владение четырьмя путями текстур и независимый CPU-стенд
между x86 и x64. Дополнение меняет wire version и требует новой согласованной пары.
