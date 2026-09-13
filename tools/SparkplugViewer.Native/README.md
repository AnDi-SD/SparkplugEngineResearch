# SparkplugViewer.Native

Наш C ABI поверх общих восстановленных C++-классов `Sparkplug/` и `Winx/`. Мост предоставляет handles, проверку входных размеров и управление временем жизни. Он не загружает игровой EXE.

## Интерфейс

[ViewerBridge.h](ViewerBridge.h) объявляет операции контейнера, объектного графа, mesh/texture inspection, анимации, узлов, skin и подготовки сцены. Дополнительные заголовки выделяют группы ABI. Managed-обвязка находится в `tools/SmoViewer/SmoViewer.Sparkplug`, Python-обвязка — в `tools/SanToVmd/sparkplug_native.py`.

Индексы ABI — host-индексы, а не оригинальные указатели. Входные массивы заимствуются на время вызова; runtime/clip handles владеют соответствующим состоянием. Исключения C++ перехватываются на границе ABI. SafeHandle и сериализованный native gate находятся в managed-слое.

Общие C++-классы читают и исполняют поддерживаемую игровую логику. Envelope encoding, транспорт данных и другие собственные host-операции маркируются отдельно. Полный перечень границ конкретного вызова определяется его объявлением и реализацией; наличие моста не означает поддержки всех классов и всех ветвей.

## Сборка

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/SparkplugViewer.Native/Build-Native.ps1 -Configuration Release -BuildWorkers 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/SparkplugViewer.Native/Build-Native.ps1 -Configuration Release -BuildWorkers 1 -RunChecks -CheckSuites FullLoader
```

Команды выполняются из корня репозитория. Нужны Visual Studio C++, Windows SDK и CMake. DLL x64 со статическим CRT создаётся в `artifacts/native/viewer/Release/`. `-Fresh` обновляет CMake-конфигурацию; `-BuildWorkers` принимает 1…4. `-CheckSuites` выбирает затронутые suites из списка в скрипте.

Если автоматический поиск Visual Studio недоступен, `-VisualStudioPath` задаёт существующую установку явно. Приложение должно использовать DLL, совместимую с текущей обвязкой.

[Архитектура общего ядра](../shared-core.md) · [Правила разработки](../../CONTRIBUTING.md).
