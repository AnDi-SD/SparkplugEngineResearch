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
