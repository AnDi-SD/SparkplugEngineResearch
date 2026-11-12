# wxSpiderAttackAIAction

PC class ID `71237CFA`, регистрационная база `wxAIAction` (`490A6EB5`).
Primary table `007095F8`, literal RTTI getter `005B33E0`,
record `007673D0`. Factory `005B3700` передаёт allocation receiver
в constructor `005B34E0`, который вызывает `wxAIAction` constructor
с тем же receiver. Наблюдаемый native base prefix — `3A8`, полный captured
объект — 988 байт. Portable raw-снимок и C++ base/layout разделены.

Полный helper `005B3470` имеет 102 байт и
аналитическое имя `StateGateForAnalysis`. Реальный typed Update
`005B3CB0` выбирает его при `3AC=2`,
через call `005B3CD2` с неизменённым receiver.
Прочие ветви Update не приписываются этой операции.

Собственный байт `3BC` читается перед owner; `actor130` command читается
только в ветви нулевого `3BC`. Command-байт — `20`.
Общая семантика четырёх ветвей, неизвестные роли, условия допуска и границы
trace/ABI описаны в [контракте state-gates](../../game/ai/attack-state-gates.md).
Остальные native-методы, ownership/lifecycle, PS2, gameplay и целый класс открыты.

Исходники: [класс](../../../Winx/Code/wxSpiderAttackAIAction.h),
[операция](../../../Winx/Code/wxSpiderAttackAIAction.cpp),
[PC-смещения](../../../Winx/Analysis/PC/wxSpiderAttackAIActionAbi.h).
