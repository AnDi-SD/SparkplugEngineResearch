# `wxIdleAIAction`

`wxIdleAIAction` (`3EA07007`) имеет регистрационную и физическую базу
`wxAIAction`. Размер native allocation — PC `3A8`, PS2 `3AC`; собственных
полей сверх базы в этих конструкциях нет. Восстановленная часть находится
в [общем исходнике](../../../Winx/Code/wxIdleAIAction.cpp), адреса и layout
отделены в [PC ABI](../../../Winx/Analysis/PC/wxIdleAIActionAbi.h) и
[PS2 ABI](../../../Winx/Analysis/PS2/wxIdleAIActionAbi.h).

## Entry и поиск цели

PC `5D66A0` и PS2 `25AF10` получают объект команд через
`action+20 → owner+144 → character+130` на PC и
`action+24 → owner+154 → character+13C` на PS2.
Если command ненулевой, вызывается его настоящий reset virtual.
Затем Entry вызывает собственный query virtual **при любом значении command**.
Аргумент native Entry не используется.

Query PC `5B5100` / PS2 `25AE00` сначала читает owner permission byte
`14D/15D`. При нуле дальнейших вызовов нет. При ненулевом значении получается
связанный perception через Character и вызывается
[поиск ближайшей цели](wx-perception.md). Найденная цель вызывает owner selector
с key `2`, parameter `0`. Owner для selector заново читается из action после
query; ранний owner используется только для permission и поиска perception.

Переносимая реализация вызывает общий `wxPerception` напрямую. Для PC явно
выбирается конечный nearest64 профиль, для ограниченной PS2-интеграции —
точный целочисленный профиль. Позиции обязаны удовлетворять выбранному
численному контракту; профиль не добавляет неизвестную EE arithmetic.
Внешние owner/Character, command и selector предоставляются обязательным
[host-контрактом](../../../Winx/Analysis/Host/wxIdleAIActionHost.h).
Он возвращает настоящий объект perception, а не придуманный результат поиска.

Объект команд здесь остаётся непрозрачным: его native table — PC `703090`,
PS2 `494710`, class name не установлен. Его точечный reset и границы
подготовки описаны отдельно в [контракте command reset](../../game/ai/ai-command-reset.md).
Переносимая Idle-логика вызывает reset через внешний адаптер.

## No-op hooks и Copy

Собственные Notify (`5B7A00/25AEE0`) и Clear (`48EAA0/25AF00`) ничего не
делают. Поэтому Idle не наследует пересылку уведомлений и очистку слова
`374/378` из общей базы. Сообщение `1C`, заимствованный current action и
clearable word сохраняются.

Copy (`5D67B0/25AE70`) вызывает общий base Copy и no-op завершение с class ID;
собственный и базовый runtime payload не переносится. Destination current и
owned actions очищаются общей реализацией `wxAIAction`; остальные поля
destination сохраняются. Clone создаёт новый Idle и получает constructor
defaults базы. Заимствованный owner и runtime-поля источника не копируются.

`WinxIdleAIActionTests` проверяет command/query порядок, оба значения
permission, отсутствие command, настоящую общую perception-операцию,
перечитывание owner, no-op hooks, Copy и владение общей базы, clone и RTTI.
Native Entry с пустым perception и Query с selector boundary квалифицируются
раздельно. Полная доставка selector, AI application, обычная сценовая
подготовка, PS2 Copy и PS2 constructor/lifetime остаются открытыми.
