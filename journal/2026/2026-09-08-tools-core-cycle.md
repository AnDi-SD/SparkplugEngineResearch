# Цикл ядер tools до 07:30 МСК, 9 сентября

Поручение: перевести ядра всех tools на одну реконструкцию игровой/движковой
логики; UI вторичен. Недостающие используемые методы исследуются в приоритете.
Если ядра готовы до срока — продолжить последовательное исследование EXE.

Первый блок около 22:55 МСК: CollisionInfo/OBB core, Node ownership и единый
обход отношений Node-derived serializers. Восемь bounded original-PC micro
cases, полный original-PC load C++-written FFPS,55 C++ checks и шесть выбранных
suites прошли. Whole native load освободил31/31 allocations. Полный corpus и
collision queries не заявлены готовыми. Release не упаковывался.

Досье: `docs/research/tools-core-cycle-2026-09-09-0730.md`, детали
`docs/research/native-pc-collision-tools-core-2026-09-08.md`, snapshot
`research/tools-core-collision-block-2026-09-08.json`.

Следующий самостоятельный шаг — использовать существующий native SAN sampler
в SanToVmd, затем продолжить SMO graph. GitHub publication остаётся отдельно
от локальных checkpoint commits; прежнее решение о публикации не изменено.
