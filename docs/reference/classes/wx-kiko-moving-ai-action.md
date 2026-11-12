# wxKikoMovingAIAction

Class ID `99887766`, регистрационная база `wxAIAction` (`490A6EB5`).
PC primary `709070`, RTTI getter `5AD0F0` → record `767130`, factory `5AD1B0`.
Native extent `1BF8` (7160 байт); собственный tail начинается с `3A8`.
Constructor `5AD030` передаёт тот же receiver в base constructor `58FF00`,
после чего устанавливает primary. Это основание для физического prefix;
portable C++ facade хранит native bytes отдельно от своих полей и containers.

Primary cell `04` выбирает полный PC-обработчик `5AD180` длиной 35 байт,
шесть инструкций. Полный DWORD входа сравнивается с `000027AA`; при совпадении
записываются raw DWORD `3A8=3`, затем `3AC=2`. Иные коды сохраняют весь raw state.
У обработчика нет callees, чтений global state или FPU-операций.

Контракт, вход и неизвестные границы описаны в
[статье о сообщении KikoMoving](../../game/ai/kiko-moving-message.md).
[Исходник](../../../Winx/Code/wxKikoMovingAIAction.h) содержит собственный
`HandleMessageForAnalysis` с явным default-refusal профилем.
[PC layout](../../../Winx/Analysis/PC/wxKikoMovingAIActionAbi.h) не задаёт layout
portable объекта. `ResultForAnalysis` относится к нашему допуску; native EAX
диагностический, native C++ return type не установлен. Generic notification,
движение, Entry/Exit/Copy/Clone, constructor/lifetime и весь класс остаются открыты.
