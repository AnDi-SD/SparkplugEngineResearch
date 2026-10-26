# wxCreditsMenuGameFlowState

`wxCreditsMenuGameFlowState` (`C564A54A`) физически наследует
[wxGameFlowState](wx-game-flow-state.md). Размер PC и PS2 — `0x44` байт.
Исходные имена методов не восстановлены; slot names и `ForAnalysis` обозначают
переносимый аналитический API. Смещения, tag и коды ниже записаны шестнадцатерично.

| Own поле | Конструктор и назначение в проверенных операциях |
| --- | --- |
| `3C` | Borrowed input из singleton `member18`; читается заново после конструктора базы |
| `40` | Нулевой byte; разрешение обработать следующий ненулевой input `2A` |
| `41` | Byte `1`; одноразовый внешний запрос с аргументом `50` |
| `42..43` | Конструктор не записывает; нулевые defaults не приписываются |

Реализация находится в
[wxCreditsMenuGameFlowState.h](../../../Winx/Code/wxCreditsMenuGameFlowState.h) и
[wxCreditsMenuGameFlowState.cpp](../../../Winx/Code/wxCreditsMenuGameFlowState.cpp).
Обязательные внешние операции определяет
[wxCreditsMenuGameFlowStateHost](../../../Winx/Analysis/Host/wxCreditsMenuGameFlowStateHost.h).
Нативные адреса и layout отделены в
[PC ABI](../../../Winx/Analysis/PC/wxCreditsMenuGameFlowStateAbi.h) и
[PS2 ABI](../../../Winx/Analysis/PS2/wxCreditsMenuGameFlowStateAbi.h).

## Constructor, Copy и lifetime

PC-конструктор вызывает общую базу, записывает own defaults, отдельно читает
input для `3C` и регистрирует receiver с tag `13` у subscription manager.
Деструктор снимает эту регистрацию и уничтожает базу с её owned timer.
Own input остаётся borrowed: отдельный AddRef/Release для `3C` не вводится.

Copy — унаследованный no-op `spBaseObject`. Он сохраняет flags, input и timer
существующего назначения. Clone строит новый derived object, регистрирует
пару в общем clone manager и вызывает этот Copy. Поэтому у clone остаются
новые `00/01`, текущая input binding и отдельный timer. Input `30` базы и
own input `3C` загружаются отдельно и не считаются одним cache-полем.

## Entry, Exit и Tick

Entry (`FlowSlot20ForAnalysis`) вызывает внешний audio interface getter и
его controller с аргументом `1`; затем пишет ноль в shared byte `45`,
вызывает HUD operation с `0`, выполняет input Reset и mode `0`, после чего
записывает `41=1`. Input берётся из `member1C` owner, захваченного до Reset;
перед mode этот member читается повторно. Замена глобального owner внутри
Reset не меняет уже захваченный owner.

Exit (`FlowSlot2CForAnalysis`) выполняет только Reset и повторное чтение
input перед mode `0`. PC-реализация использует общий code helper, на который
ссылается также LogoAnim state. Empty hooks `24/28` сохраняются в базе.

Tick сначала проверяет `41`. Если byte ненулевой, он очищается **до** внешнего
запроса `50`. После callback flags и cached input читаются заново. При
ненулевых `40` и input byte `2A` Tick очищает `40` и выполняет completion
sequence. Иначе он повторно читает input `2A`; ноль устанавливает `40=1`.
Повторное чтение не заменяется сохранённым первым результатом. Ненулевые raw
bytes не нормализуются в `bool`, если исходная операция их не переписывает.

## Completion и notification

Операция PC `5EC5A0` реализована непосредственно в общем коде. Её исходное
C++-имя/объявление неизвестно; аналитический метод не является callback,
заменяющим эту логику. Порядок следующий:

1. Прочитать controller state `1B0`; только при `4A` запросить `3C,0`.
2. После этого optional callback заново разрешить controller; установить
   `38,0`. Без optional callback используется уже захваченный controller.
3. Заново разрешить service и отправить `2BC,0`.
4. Заново разрешить input owner и записать его byte `20=1`.

Notification `27DB` с числовым payload `50` вызывает ту же последовательность,
сохраняя own flags. Другие payload игнорируются; другие codes проходят в
базовый no-op. Переносимый virtual boundary использует общий
`spNotificationForAnalysis`; числовой Credits payload ограничен исходным
32-битным словом. Native byte layout пакета не приравнивается к host layout.

## Проверка и оставшиеся границы

`WinxCreditsMenuGameFlowStateTests` проверяет raw defaults, no-op Copy,
fresh-default Clone, listener ownership, stores до callbacks, замену input/
controller/service/owner внутри callbacks и точный порядок действий.

PC-сравнение исполняет owned Entry/Exit/Tick/Notify/completion на исходных
instructions. Чужие audio/HUD/input/controller/service bodies задаются
обязательными внешними контрактами, а completion helper исполняется целиком.
Subscription-manager prerequisite и его register/unregister bodies выполняются
оригинальным кодом.

Для PS2 отдельно различаются завершённые no-consumer calls, остановки на
входе foreign consumers и свежие частичные входы с явно заданным состоянием
после предыдущего callback. Последние не доказывают, что предыдущий callee
создал эти inputs, и не объединяются в полную операцию. Own integer sequence
сохраняется, но конструктор общей реализации использует PC parent semantics.
PS2 parent globals, heap/lifetime, аппаратная EE семантика и живые credits/UI
остаются открытыми.
