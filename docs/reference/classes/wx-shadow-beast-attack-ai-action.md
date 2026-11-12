# wxShadowBeastAttackAIAction

PC class ID `0F6C5372`, регистрационная база `wxAIAction` (`490A6EB5`).
Primary table `00709B60`, literal RTTI getter `005B97E0`,
record `00767790`. Factory `005B9AB0` передаёт allocation receiver
в constructor `005B9900`, который вызывает `wxAIAction` constructor
с тем же receiver. Наблюдаемый native base prefix — `3A8`, полный captured
объект — 992 байт. Portable raw-снимок и C++ base/layout разделены.

Полный helper `005B9890` имеет 100 байт и
аналитическое имя `StateGateForAnalysis`. Реальный typed Update
`005BA600` выбирает его при `3AC=3`,
через call `005BA63D` с неизменённым receiver.
Прочие ветви Update не приписываются этой операции.

Command-pointer `actor130` читается безусловно перед собственным байтом
`3BC` и state-record. Command-байт — `22`; последняя собственная DWORD-запись
при сбросе — `3B0=0`, после `3AC` и `3B4`.
Общая семантика четырёх ветвей, неизвестные роли, условия допуска и границы
trace/ABI описаны в [контракте state-gates](../../game/ai/attack-state-gates.md).
Остальные native-методы, ownership/lifecycle, PS2, gameplay и целый класс открыты.

Исходники: [класс](../../../Winx/Code/wxShadowBeastAttackAIAction.h),
[операция](../../../Winx/Code/wxShadowBeastAttackAIAction.cpp),
[PC-смещения](../../../Winx/Analysis/PC/wxShadowBeastAttackAIActionAbi.h).
