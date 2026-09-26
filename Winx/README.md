# Исходники Winx Club

Восстановленная игровая логика поверх Sparkplug: типы `wx…`, игровые состояния, персонажи и связи с общими подсистемами движка. Это отдельный игровой слой; базовые типы `sp…` находятся в [Sparkplug](../Sparkplug/README.md).

Код в `Code/` охватывает известные части поведения. Наличие типа или конструктора не означает готовности всей игровой системы. Общие приложения подключают этот код напрямую либо через native-мост.

[Устройство игры](../docs/game/README.md) · [Каталог игровых классов](../docs/reference/game-classes.md) · [Правила разработки](../CONTRIBUTING.md).

## Состояния персонажа

`WinxCharacterStates` содержит общую реализацию [wxActionState](../docs/reference/classes/wx-action-state.md) и используемых hooks [wxCharacterState](../docs/reference/classes/wx-character-state.md). Она зависит только от `SparkBase`; `WinxGameCore` подключает её публично. Внешние объекты анимации передаются через наш [host-интерфейс](Analysis/Host/wxCharacterStateHost.h), без реализации по умолчанию. Это компонент, не готовая state machine игры.

Сборка и проверки из корня репозитория в терминале с настроенным C++ toolchain:

```powershell
cmake -S Winx -B .codex-tmp/Winx-state-build -DBUILD_TESTING=ON
cmake --build .codex-tmp/Winx-state-build --config Debug --target WinxActionStateTests --parallel 1
ctest --test-dir .codex-tmp/Winx-state-build -C Debug -R '^WinxActionStateTests$' --output-on-failure
```

Тест проверяет полный цикл переходов, маски key, порядок stop/fade/play, повторный handle, clone и RTTI. Оригинальные игровые файлы для него не нужны.

`WinxAIActions` содержит подтверждённое базовое ядро [wxAIAction](../docs/reference/classes/wx-ai-action.md): lifetime, Copy/clone, notifications и простые hooks. Два крупных movement-метода и производные действия пока остаются явно документированной границей. Их узкая проверка собирается целью `WinxAIActionTests`.

`WinxAIManagers` содержит [wxAIManager](../docs/reference/classes/wx-ai-manager.md): владение списком, ограниченный обход AI, сообщения и сценарии уровней. Внешние игровые объекты подключаются через обязательный `wxAIManagerHost`; PC/PS2 layout хранятся отдельно. Для сборки и проверки используйте те же команды с целью и именем теста `WinxAIManagerTests`. Тест охватывает обход, удаление из callback, clone/Copy, RTTI и последовательность падения/возврата игрока.

`WinxDoorTriggers` содержит собственную логику [wxAlfeaDoorTrigger](../docs/reference/classes/wx-alfea-door-trigger.md): групповые сообщения, очередь открытия, фазы поворота, условия взаимодействия и Copy/clone. Ещё не восстановленная `wxPivotingDoor` и внешние игровые системы подключаются через `wxAlfeaDoorTriggerHost`. Цель и имя узкой проверки — `WinxAlfeaDoorTriggerTests`; она не требует игровых файлов.

`WinxAlphaManagers` содержит [wxAlphaManager](../docs/reference/classes/wx-alpha-manager.md): пул, эффекты цвета и прозрачности, восстановление материалов, классификацию узлов и затемнение экрана. Внешние объекты и renderer подключаются через обязательный `wxAlphaManagerHost`. Цель сборки и имя теста — `WinxAlphaManagerTests`; игровые файлы для него не нужны.

`WinxAnimationControllers` содержит [wxAnimationController](../docs/reference/classes/wx-animation-controller.md): команды actor, историю запусков, отметки завершения, переключение flags и доставку тегов состоянию/звуку. Использует существующий тип запроса `spActor`; недостающая база и внешние объекты подключаются через `wxAnimationControllerHost`. Цель сборки и имя теста — `WinxAnimationControllerTests`.

`WinxAnimationLoaders` содержит [wxAnimationLoader](../docs/reference/classes/wx-animation-loader.md): выбор наборов анимаций по уровню, обработку уведомления загрузки и освобождение при уничтожении. База, игровой контекст и `wxAnimationManager` подключаются через `wxAnimationLoaderHost`. Цель сборки и имя теста — `WinxAnimationLoaderTests`; оригинальные игровые файлы для этого теста не нужны.

`WinxAnimationManagers` содержит [wxAnimationManager](../docs/reference/classes/wx-animation-manager.md): кэш ресурсов, загрузку 68 таблиц наборов, преобразование полей `.anm` в ключ и поиск с fallback. Файлы, потоки и ресурсы подключаются через `wxAnimationManagerHost`; [wxAnimationLoaderManagerHost](Analysis/Host/wxAnimationLoaderManagerHost.h) связывает восстановленные загрузчик и менеджер. Цель сборки и имя теста — `WinxAnimationManagerTests`; проверка включает их совместный цикл загрузки и освобождения и не требует игровых файлов.

`WinxAppHelpers` содержит [wxAppHelper](../docs/reference/classes/wx-app-helper.md): настройки PC/PS2, пути и операции PCK, этапы запуска, обновление и завершение подсистем. Внешние службы подключаются через обязательный `wxAppHelperHost`. Цель сборки и имя теста — `WinxAppHelperTests`; проверка не требует игровых файлов и включает завершение с удалением самого helper.
