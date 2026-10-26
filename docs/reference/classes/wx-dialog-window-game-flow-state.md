# wxDialogWindowGameFlowState

`wxDialogWindowGameFlowState` (`4D6F71CC`) физически наследует
[wxGameFlowState](wx-game-flow-state.md) без обнаруженного собственного
хвоста. Размер выделения PC и PS2 — `0x3C`. PC Clone строит новый объект,
регистрирует его в общем clone manager и вызывает унаследованный no-op Copy;
parent state и owned timer не переносятся из источника.

Общий код —
[wxDialogWindowGameFlowState.h](../../../Winx/Code/wxDialogWindowGameFlowState.h) и
[wxDialogWindowGameFlowState.cpp](../../../Winx/Code/wxDialogWindowGameFlowState.cpp).
Borrowed window storage и внешний consumer предоставляет
[wxDialogWindowGameFlowStateHost](../../../Winx/Analysis/Host/wxDialogWindowGameFlowStateHost.h).
Нативные описания —
[PC ABI](../../../Winx/Analysis/PC/wxDialogWindowGameFlowStateAbi.h) и
[PS2 ABI](../../../Winx/Analysis/PS2/wxDialogWindowGameFlowStateAbi.h).

## Window hooks

Hook PC `24` разрешает window object и обнуляет ровно `0x258` байт в его
диапазоне `[14,26C)`. PC использует `REP STOSD` со счётчиком `150`; PS2
передаёт `window+14`, `0`, `258` в `memset`. Байты до `14` и начиная с `26C`
не входят в reset. Аналитический host предоставляет отдельный borrowed
`std::array<uint8_t,0x258>` для этого диапазона, а восстановленный класс сам
выполняет его заполнение.

Hook PC `30` разрешает window и вызывает его consumer `5C98B0`; PS2
соответствует consumer `36C400`. После внешнего вызова обе исходные версии
записывают литеральный `true`, не используя результат consumer. С учётом
PS2 table header этим hooks соответствуют смещения `2C` и `38`.

Эти операции не читают активный input `member1C`, который используют
DebugMenu/LoadSave states. Общий parent constructor по-прежнему имеет свою
borrowed input связь и отдельный timer.

## Границы

Конструктор и lifetime общей реализации соответствуют PC-базе. Разрешение
window singleton и тело его consumer обязательны через host и остаются
внешними операциями; успешных default callbacks нет. Недоступный token
вызывает собственный guard аналитического API, а не воспроизведение
нативного поведения ошибки.

Проверка — `WinxDebugDialogGameFlowStateTests`. PC reset сравнивается целиком
с оригинальными изменёнными байтами и guards на четырёх исходных заполнениях.
PS2 reset пока сравнивается только до входа оригинального `memset`: его
аргументы подтверждены, исполнение тела и конечные PS2 байты отдельно не
квалифицированы. Window consumers проверяются до настоящего входа, после
чего guest отбрасывается без продолжения. Полный PC factory/RTTI/Clone/delete
проверяется отдельным свежим опытом. PS2 constructor/heap/lifetime,
аппаратная семантика и живой dialogue UI остаются открытыми.
