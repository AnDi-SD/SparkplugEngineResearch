# wxIceWormHolesAIAction

Class ID `812B81AB`, регистрационная база `wxAIAction` (`490A6EB5`).
PC primary `6F9D78`, RTTI getter `522410` → record `751068`.
Типизированное captured-состояние занимает `3C8` (968) байт.
Физическое C++-наследование не установлено: защищённые входы factory
`403FE0` и constructor `522370` не дают доказательства same-receiver prefix.

Primary cell `04` выбирает обработчик `522490` размером 204 байта.
У него есть ветвь с foreign Node-вызовами и `4365E0`. Профиль
`NoNodeCalls` принимает остальные ветви: code DWORD `281C` записывает
byte `3C4=1`, `281D` записывает byte `3C4=0`, `2739` при нулевом
low byte payload DWORD `18` записывает DWORD `3A8=2`.
Иные code DWORD сохраняют raw state. Весь обработчик call-free не объявляется.

[Контракт входа и границы](../../game/ai/ice-worm-holes-message.md).
[Исходник](../../../Winx/Code/wxIceWormHolesAIAction.h) — наш plain captured
facade без наследования, native vptr и RTTI factory.
[PC offsets](../../../Winx/Analysis/PC/wxIceWormHolesAIActionAbi.h)
относятся к captured bytes, а не к layout portable объекта.
Generic dispatcher, foreign ветвь, движение, lifecycle и весь класс остаются открыты.
