# Работа в VS Code

Откройте корень репозитория в VS Code. Рекомендуемые расширения перечислены в `extensions.json`. Текущий C# Dev Kit требует .NET SDK 10; для сборки `.slnx` без этого расширения достаточно SDK 9.0.300 или новее. CMake Tools настроен на `Winx/CMakeLists.txt` и выводит сборку в игнорируемый `.codex-tmp/vscode-winx/`.

`Ctrl+Shift+B` последовательно собирает native-мост, SmoViewer и SMOTextureTool. Отдельные задачи, включая FBX-мост при наличии Autodesk FBX SDK, доступны через **Terminal → Run Task**. Для `Winx/` выберите обнаруженный MSVC x64 kit в CMake Tools, затем используйте **CMake: Configure**, **CMake: Build** и **CMake: Run Tests**. Генератор Ninja использует окружение выбранного kit.

VS Code работает как редактор и интерфейс запуска. Для сборки нужны .NET SDK, CMake, Windows SDK и MSVC x86/x64. Компилятор и его окружение доступны в отдельном **Visual Studio Build Tools 2026**; Visual Studio Community как IDE не требуется после переноса и проверки. Скрипты проекта находят Build Tools через `vswhere` и сами загружают окружение компилятора.

Для работы после удаления Visual Studio Community установите Build Tools с нагрузкой **Desktop development with C++** и компонентами MSVC x86/x64, CMake tools for Windows и Windows 11 SDK 10.0.26100. Проверьте сборку native-моста, тест `WinxBacoManagerTests`, обе .NET-программы и FBX-мост при наличии Autodesk FBX SDK. Если пользовательский `PATH` указывает на CMake внутри Community, после установки Build Tools замените эту запись на путь к его CMake. Для сборки `.slnx` целиком нужен .NET SDK 9.0.300 или новее; задачи VS Code собирают отдельные `.csproj`, доступные и с SDK 8.
