# Исходники Sparkplug

Общая восстановленная реализация движка. Её используют приложения из `tools/`; форматные и игровые алгоритмы не должны дублироваться в каждом приложении. Восстановление отдельных классов и методов продолжается.

[spInputDevice](../docs/reference/classes/sp-input-device.md) маршрутизирует
подтверждённые логические запросы к физическим кодам. Родственные
[spDXInputDevice](../docs/reference/classes/sp-dx-input-device.md) и
[spPS2InputDevice](../docs/reference/classes/sp-ps2-input-device.md) сохраняют
собственные начальные поля и установленные операции. Аппаратный ввод и
владение DX-интерфейсами задаются явными host-границами.
Проверка — `SparkplugInputDeviceTests`.

[spPS2Keyboard](../docs/reference/classes/sp-ps2-keyboard.md) сохраняет
подтверждённые начальные поля и короткие тела PS2-операций, включая исходную
инициализацию и привязку к общим запросам ввода. Проверка — `SparkplugPS2KeyboardTests`.

[spVideoStream](../docs/reference/classes/sp-video-stream.md) и
[spPCVideoStream](../docs/reference/classes/sp-pc-video-stream.md) сохраняют
собственный lifetime буфера, clone и реальные короткие ответы PC-интерфейса.
Имена исходных операций и декодирование остаются открытыми.
Проверка — `SparkplugVideoStreamTests`.

[spCubeTexture](../docs/reference/classes/sp-cube-texture.md),
[spDXCubeTexture](../docs/reference/classes/sp-dx-cube-texture.md),
[PC-сериализатор](../docs/reference/classes/sp-dx-cube-texture-serializer.md) и
[spPS2CubeTexture](../docs/reference/classes/sp-ps2-cube-texture.md) используют
общие потоки и объектную сериализацию. PC сохраняет raw `D3DFORMAT`, шесть
граней и исходные особенности строк DXT. После чтения нескольких mip-уровней
оригинал регенерирует их; переносимая операция требует явного provider этой
генерации. Самостоятельно проверенная операция — один уровень; проверка —
`SparkplugCubeTextureTests`, включая запись и повторное чтение через общий FAT.

[spLightController](../docs/reference/classes/sp-light-controller.md)
обновляет цвет Light через общие ColorFuncEval и FunctionEval, добавляет dirty
бит и сохраняет исходные правила clone. Проверка — `SparkplugLightControllerTests`.
[spQuad3D](../docs/reference/classes/sp-quad-3d.md) строит ориентированный по
камере квадрат и передаёт буферы и параметры отрисовки явному renderer-host;
проверка — `SparkplugQuad3DTests`.
[spSystemSettings](../docs/reference/classes/sp-system-settings.md) сохраняет
подтверждённые собственные байты, singleton и clone; назначение настроек
остаётся открытым. Проверка — `SparkplugSystemSettingsTests`.

`SparkEntities` содержит подтверждённые создание, RTTI и назначение ссылки [spEntity](../docs/reference/classes/sp-entity.md). Менеджер и составной destructor передаются явному host; узкая проверка — `SparkEntityTests`.

`SparkNetwork` содержит [spDXNetwork](../docs/reference/classes/sp-dx-network.md) и [базу spNetwork](../docs/reference/classes/sp-network.md): сокетные методы PC, общий copy имени и жизненный цикл. Вызовы WinSock предоставляет явный общий host. Независимые проверки базы — `SparkplugNetworkTests`, платформенного класса — `SparkplugDXNetworkTests`. Компонент входит в зависимости `SparkplugEngine`.

В `SparkNetwork` также находится [spNetworkDebug](../docs/reference/classes/sp-network-debug.md): PC-журнал, дамп пакета, частота кадров и жизненный цикл. Внешние часы, поток, блокировка и вывод назначаются через отдельный host. Проверка — `SparkplugNetworkDebugTests`.

[spNetworkManager](../docs/reference/classes/sp-network-manager.md) восстанавливает PC-запуск и остановку сетевых служб, ошибки, очередь, probe/Hello, уведомления и ввод matchmaking. Внешние службы предоставляет `spNetworkManagerHost`; проверка — `SparkplugNetworkManagerTests`.

[spNetworkMatchmaking](../docs/reference/classes/sp-network-matchmaking.md) содержит PC-жизненный цикл, закрытие соединения и обработку входящих данных с уведомлением `0x22`. Он использует общий `spMemoryStream` и явный `spNetworkMatchmakingHost` для socket service и вывода; проверка — `SparkplugNetworkMatchmakingTests`.

[spStreamError](../docs/reference/classes/sp-stream-error.md) и
[spWindowsError](../docs/reference/classes/sp-windows-error.md) сохраняют
родственные `spError` форматирование, RTTI и жизненный цикл. Переносимые строки
отделены от общего временного буфера оригинала. Проверка — `SparkplugDerivedErrorTests`.

[spTimer](../docs/reference/classes/sp-timer.md) и
[spMasterTimer](../docs/reference/classes/sp-master-timer.md) содержат общую
логику часов и дочерних обновлений; аппаратный источник времени задан явной границей.
[spTransformConstEval](../docs/reference/classes/sp-transform-const-eval.md)
сохраняет вычисление transform с документированными численными границами PC.

[spSubtitleTrack](../docs/reference/classes/sp-subtitle-track.md) загружает
собственный blob и таблицы и сохраняет исходный порядок поиска записей.
Состав записи описан в [формате subtitle track](../docs/formats/subtitle-track.md).

[spThread](../docs/reference/classes/sp-thread.md) задаёт общий интерфейс;
[spPCThread](../docs/reference/classes/sp-pc-thread.md) сохраняет семь собственных
операций PC и их обработку OS результатов. Платформенные вызовы требуют
`spPCThreadHost`; реальное исполнение потока предоставляет адаптер.
Проверка — `SparkplugThreadTests`.

| Каталог | Содержание |
| --- | --- |
| `Code/SparkBase` | Базовые объекты, память, потоки и служебные типы |
| `Code/SparkBasePC` | PC-реализации базового слоя |
| `Code/Sparkplug` | Ресурсы, сериализация, сцена, анимация и остальные общие подсистемы |
| `Code/SparkplugDX` | Поведение исходного DirectX backend |
| `Code/SparkplugPC` | PC-приложение и платформенные загрузчики |
| `Analysis/PC`, `Analysis/PS2` | Описания ABI и вспомогательные части восстановления для соответствующей платформы |
| `Analysis/Host` | Наш общий слой совместимости; не исходная игровая реализация |

Часть `Analysis/` участвует в сборке общего ядра. Это исходный код, поэтому каталог остаётся рядом с `Code/`. Название восстановленного файла не всегда означает, что известен точный оригинальный путь; такие границы отмечены в исходниках.

Начать чтение: [архитектура](../docs/engine/architecture/overview.md), [подсистемы](../docs/engine/README.md), [каталог классов](../docs/reference/classes.md). Сборка общего C++-моста: [SparkplugViewer.Native](../tools/SparkplugViewer.Native/README.md). Обязательные [правила разработки](../CONTRIBUTING.md).
