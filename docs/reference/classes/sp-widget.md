# spWidget

`spWidget` (`0x7CB52800`) наследует [spGUIObject](sp-gui-object.md)
(`0x7339573D`). [Общие исходники](../../../Sparkplug/Code/Sparkplug/spWidget.h)
восстанавливают выбор представления из normal, disabled и highlighted Nodes,
enabled/visible setter dispatch и известную часть copy/clone.

PC extent `0x54`, PS2 `0x50`. После GUI prefix находятся raw bytes visible
и highlight (`1` и `0`), selected Node, три state Nodes и selection index.
Все четыре указателя и index первоначально нулевые. State Nodes и selected
Node заимствуются: выбор и shallow copy не приобретают strong references.
Точный layout приведён в [PC](../../../Sparkplug/Analysis/PC/spGUIAbi.h) и
[PS2](../../../Sparkplug/Analysis/PS2/spGUIAbi.h) ABI records.

## Выбор состояния

Если прежний selected Node существует и его EnabledMask уже сброшен,
операция сразу возвращается. Выбор, index и остальные Nodes не меняются.
Иначе прежний Node рекурсивно выключается до выбора нового.

| Условие | Index | Выбранный Node |
| --- | ---: | --- |
| enabled byte равен нулю | `1` | disabled |
| enabled ненулевой и highlight ненулевой | `2` | highlighted |
| enabled ненулевой и highlight равен нулю | `0` | normal |

Если выбранный state Node отсутствует, используется normal Node. Index при
этом остаётся индексом исходного выбора. Если отсутствует и normal, никаких
Node calls не происходит. Новый selected Node получает raw visible byte
и recursive flag `1`; его native EnabledMask зависит от ненулевого значения.

Даже при выборе того же Node он сначала выключается, затем включается с
текущим visible byte. Enabled и visible setters сначала записывают low byte,
затем виртуально вызывают выбор состояния. Поэтому наследник
[spButton](sp-button.md) получает собственный pressed selector через те же
setters. Базовый setter GUIObject, меняющий collision groups, здесь не вызывается.

## Нотификации

Известная PC-часть Notify сохраняет исходный message object при пересылке.
При ненулевом enabled сообщения `0x0E`, `0x0F`, `0x10` передаются текущему
notification target из Entity `+0x24`. Сообщения `0x11` и `0x12` сначала
записывают highlight `1` или `0`, вызывают виртуальный Refresh, затем заново
читают target и передают ему тот же исходный message. Enabled после Refresh
в этой ветви повторно не проверяется.

После own-части вызывается базовый GUIObject Notify. Он видит актуальный
message, включая изменения code/payload, сделанные callback. Зарезервированные
поля, sender, payload и tail пересылаемого сообщения не пересобираются.
Остальные обычные коды попадают прямо в базовый handler; `0x18` возвращает
указатель на этот объект.

Прямой Widget owner call в оригинале разыменовывает target. Portable guard
сообщает о нулевой зависимости, сохраняя уже выполненные изменения. Это
отличается от общего directed-message helper, который допускает null target.

### Протокол `0x1C`

При ненулевом исходном Node последовательно выполняется общий виртуальный
lookup `NORMAL`, `DISABLED`, `HIGHLIGHTED`, с recursive и substring flags `1`.
Перед вторым и третьим поиском исходный Node читается заново. Результаты
сохраняются как borrowed pointers без приобретения strong references.

Всем коллизиям normal Node присваивается group `4`. Disabled Node рекурсивно
выключается. Highlighted Node рекурсивно выключается, затем его коллизии также
получают group `4`. Selected становится normal Node, включая null. Этот путь
не вызывает Refresh и не меняет visible, highlight или selection index.
Если имена разрешились в один Node, соответствующие операции повторяются.

После own-части вызывается базовый GUIObject Notify `0x1C`: он ищет
`GUICollision` и присваивает найденным collision groups значение `4`.
Исходный нулевой Node в итоге приводит native base path к разыменованию;
portable слой явно сообщает об отсутствующей зависимости.

## Копирование и открытые части

Copy сначала выполняет GUI copy, затем присваивает highlight, selected Node,
три state Nodes и index. Visible byte **не копируется**: у нового clone он
остаётся `1`, у существующего destination сохраняется прежнее значение.
Указатели копируются поверхностно и не перемапляются самим copy.

Квалификация lookup относится к стабильным графам фактических `spNode`.
Мутации children во время обхода и неизвестные derived Find overrides
остаются открытыми. Provider notification target и actual dispatch являются
явными внешними зависимостями; полный PS2 Notify и UI startup не выводятся
из общей PC-реализации. `UnrecoveredGUINotificationForAnalysis` сообщает о
непредставленной либо недопустимой зависимости, сохраняя выполненный prefix.

`SetSelectionStateForAnalysis` задаёт явное исходное runtime state для
аналитического вызова. Он не объявлен оригинальным setter и не воспроизводит
lookup, subscriptions или ownership. Наличие исходников не означает полной
реализации UI startup либо визуальной приёмки.
