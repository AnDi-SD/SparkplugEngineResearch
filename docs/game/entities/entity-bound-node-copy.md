# Привязка Node: Barrel и назначение Copy у MoveCtrl

## CharacterMoveCtrl: Copy требует Node назначения

| Имя | PC destination offset | PS2 destination offset |
| --- | ---: | ---: |
| camera_lookat_head | 2B4 | 2C0 |
| model_root_master | 2B8 | 2C4 |
| hand_right | 2BC | 2C8 |

Offsets шестнадцатеричные. На PS2 Copy `002B1E20` делает эти поиски inline:
загружает `destination+18` в `002B208C`, `002B20A8`, `002B20CC`, затем вызывает
Node virtual method через `vtable+30`. Source и destination не перепутаны:
S1 хранит источник, S0 — назначение.

Теперь установлено, что обычная схема `factory → Copy → назначить Node`
не удовлетворяет этому Copy. Как оригинальная игра подготавливает назначение
в нормальном процессе клонирования, ещё не восстановлено. Переставлять шаги
в восстановленном Clone, копировать туда source Node или подставлять пустые
результаты поиска нельзя. Эта ветвь остаётся открытой.
