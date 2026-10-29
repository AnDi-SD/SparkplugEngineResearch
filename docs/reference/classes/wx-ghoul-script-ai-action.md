# `wxGhoulScriptAIAction`

`wxGhoulScriptAIAction` (`75E255E9`) имеет регистрационного и физического
родителя [`wxAIAction`](wx-ai-action.md). PC factory `5A9860` выделяет `0x3BC`,
вызывает `5A95C0`, затем общий AIAction constructor `58FF00` на том же receiver.
Промежуточная таблица `702450` заменяется на `708D68`; getter `5A9680`
возвращает record `766F50`. [Raw32 layout](../../../Winx/Analysis/PC/wxGhoulScriptAIActionAbi.h)
отделён от переносимого C++ объекта и его указателей.

## PC Entry

Собственный Entry PCslot28/`5A9930` (переносимое имя30) один раз читает
owner из action20 и сохраняет его до конца вызова. Затем по порядку:

1. Читает owner24→entity24→Node и сохраняет указатель в action3B0.
2. Читает тот же owner144→actor130→command и сохраняет указатель в action3B8.
3. Записывает byte14D сохранённого owner в0.
4. Очищает DWORD3A8 и DWORD3AC.
5. Устанавливает базовую durationMilliseconds370 в15000.

Метод возвращается через `ret4`; аргумент не читается, bool-смысл EAX не
приписывается. Замена action20 после первого кеширования Node не меняет owner
текущего вызова. Изменения звеньев owner144/actor130 до их чтения учитываются.
Следующий целый Entry снова читает текущий action20. Нулевые значения Node
и command допустимы как данные, поскольку этот метод их не разыменовывает.

[Реализация](../../../Winx/Code/wxGhoulScriptAIAction.h) принимает явный
postconstruction capture и [storage-only host](../../../Winx/Analysis/Host/wxGhoulScriptAIActionHost.h).
Родитель и его duration setter настоящие; полный native constructor не
воспроизводится. Node — заимствованный живой `spNode*`, command — непрозрачная
заимствованная идентичность; native32 слова не преобразуются в host pointers.
Деструктор действия при живых контрольных Node/command сохраняет их байты и
не освобождает Node/его sentinel. Это ограниченная проверка заимствования,
не общий контракт графа владения или жизненного цикла исходных owner/actor.

## Допуск и неизвестные операции

Host по умолчанию `Unqualified`. Явный `PcCapturedBorrowedEntryData` требует
живых ограниченных owner/entity/actor/command записей и точных plain Node
объектов. Node type проверяется перед кешированием без виртуального вызова.
CurrentAction и owned-action список пусты. Opaque native word3B4 должен быть
явно захвачен равным0: его тип, смысл и общий constructor default не заявлены.
Native Entry это слово не читает и не меняет; наш guard отдельно проверяет0.

Чтения различают отсутствующую wrapper-запись и допустимый null payload.
При отсутствии actor после первого кеширования Node эта запись сохраняется;
`MissingActor` — аналитический частичный отказ, не native fault/rollback.
`LastResultForAnalysis` также является нашим состоянием допуска.

Контролируемые изменения входа в replay выполняются в начале ReadCommand,
после записи Node и до чтения actor того же кешированного owner. Host не
выбирает AI outcome, offsets или значения. Native CPU read/store counts
не приравниваются к числу переносимых callback/присваиваний.

Generic factory и Clone требуют неподтверждённого полного construction и
выдают явный отказ; Copy возвращает аналитический false. Notify, остальные
update/exit/query/argument hooks помечены Unqualified. Отказ в void-слоте
читается через LastResult и не означает восстановленный native no-op.
[Тесты](../../../Winx/Tests/wxGhoulScriptAIActionTests.cpp) проверяют повторы,
идентичности, полный command payload и границы допуска. Остальная state
machine, исходные типы wrappers, произвольный ownership, PS2 и gameplay
остаются открытыми; класс целиком не закрыт.
