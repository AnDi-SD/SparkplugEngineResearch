# Исходники Sparkplug

Общая восстановленная реализация движка. Её используют приложения из `tools/`; форматные и игровые алгоритмы не должны дублироваться в каждом приложении. Восстановление отдельных классов и методов продолжается.

`SparkEntities` содержит подтверждённые создание, RTTI и назначение ссылки [spEntity](../docs/reference/classes/sp-entity.md). Менеджер и составной destructor передаются явному host; узкая проверка — `SparkEntityTests`.

`SparkNetwork` содержит [spDXNetwork](../docs/reference/classes/sp-dx-network.md) и [базу spNetwork](../docs/reference/classes/sp-network.md): сокетные методы PC, общий copy имени и жизненный цикл. Вызовы WinSock предоставляет явный общий host. Независимые проверки базы — `SparkplugNetworkTests`, платформенного класса — `SparkplugDXNetworkTests`. Компонент входит в зависимости `SparkplugEngine`.

В `SparkNetwork` также находится [spNetworkDebug](../docs/reference/classes/sp-network-debug.md): PC-журнал, дамп пакета, частота кадров и жизненный цикл. Внешние часы, поток, блокировка и вывод назначаются через отдельный host. Проверка — `SparkplugNetworkDebugTests`.

[spNetworkManager](../docs/reference/classes/sp-network-manager.md) восстанавливает PC-запуск и остановку сетевых служб, ошибки, очередь, probe/Hello, уведомления и ввод matchmaking. Внешние службы предоставляет `spNetworkManagerHost`; проверка — `SparkplugNetworkManagerTests`.

[spNetworkMatchmaking](../docs/reference/classes/sp-network-matchmaking.md) содержит PC-жизненный цикл, закрытие соединения и обработку входящих данных с уведомлением `0x22`. Он использует общий `spMemoryStream` и явный `spNetworkMatchmakingHost` для socket service и вывода; проверка — `SparkplugNetworkMatchmakingTests`.

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
