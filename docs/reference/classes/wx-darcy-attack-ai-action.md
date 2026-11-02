# wxDarcyAttackAIAction

`wxDarcyAttackAIAction` (`0x590C5D10`) имеет PC extent `0x460` и префикс
[`wxAIAction`](wx-ai-action.md). В constructor prefix исходный receiver
передаётся прямо в общий constructor `58FF00`, после чего устанавливается
primary table `70A230`. [PC32 layout](../../../Winx/Analysis/PC/wxDarcyAttackAIActionAbi.h)
описывает оригинальное размещение отдельно от переносимого C++ ABI.

[Реализация](../../../Winx/Code/wxDarcyAttackAIAction.h) восстанавливает PC Entry
`5C23D0` и отдельный Exit `5C2230` на явно заданном postconstruction state. Entry использует настоящий
общий алгоритм [`wxCharacterRegistry`](wx-character-registry.md). Собственные
имена метода, полей и файлов не установлены.

## Порядок входа

Entry читает существующую Registry binding заново на каждом вызове. Селектор
вызывается перед всеми собственными записями. Его Completed-null результат
допустим. Затем последовательно записываются:

1. words `3B0 = 0`, `3B8 = 0`, byte `3AC = 0`;
2. words `3C0 = 0`, `3B4 = 0`, `3CC = 0`, `400 = 0`;
3. bytes `3F0 = 0`, `45C = 0`;
4. pointer `3A8 = selected target`;
5. words `3BC = 1`, `3D0 = 2`.

Эти литералы не получают выдуманных имён игровых состояний. Остальные
собственные байты, включая padding рядом с flags, и базовые поля сохраняются.
Owner/actor/command записи не читаются. Стековый аргумент не используется;
EAX не трактуется как boolean результата.

Source хранит target как заимствованную живую C++ identity, а остальные
`180` own bytes `3AC..45F` — как captured data. Закодированные native pointers
не преобразуются в host pointers и не становятся владельцами объектов.
[Общая binding record](../../../Winx/Analysis/Host/wxCharacterRegistryBinding.h)
содержит только указатель на существующую Registry. Она не выбирает готовую
цель и не восстанавливает producer native global или singleton lifetime.

## Границы профиля

По умолчанию профиль не разрешён. Явный PC-профиль требует существующую
admitted Registry, живые стабильные borrowed records и пустой inherited action
graph. Внешняя binding может меняться между вызовами; текущий вход захватывает
её один раз. Cache/target изменения между вызовами — отдельные captured inputs,
а не восстановленные методы reset или insertion.

Missing binding или отказ самой Registry дают аналитический отказ до own
stores. Это не модель native cold-construction, fault или rollback. Generic
factory, Copy/Clone, Notify и другие неквалифицированные hooks явно отказывают.
Portable constructor и destructor обслуживают captured view; полного native
constructor/destructor, embedded support objects или owning lifetime они не
восстанавливают. Active attack bodies, другие hooks, произвольные graphs,
PS2, холодный запуск, gameplay и полнота класса остаются открытыми.

## Выход и command storage

Exit `5C2230` заново читает всю цепочку `own20 → owner144 → actor130` перед
каждой из шести записей нуля в command bytes `1D`, `20`, `21`, `22`, `23`,
`24`. После всех шести записей он обнуляет собственный target `3A8`.
Предыдущие owner, actor и command не кэшируются для следующей записи.
Остальные command bytes, raw own state и базовые поля сохраняются.
У метода нет внешних callees, Registry или argument/result boolean contract.

Переносимый Exit получает отдельный
[storage-only host](../../../Winx/Analysis/Host/wxDarcyAttackAIActionExitHost.h).
Host предоставляет чтение borrowed actor/command cells и запись одного byte;
сам исходник владеет порядком шести чтений/записей и финальным target clear.
Смещения внешних записей не объявляют их оригинальное C++ наследование.

Exit по умолчанию не привязан и не разрешён. `BindExitForAnalysis` задаёт
живой host с отдельным явным PC-профилем и пустым inherited action graph.
Разрешение Entry не разрешает Exit автоматически и наоборот. Captured owner
задаётся явно через аналитический owner binding; нулевой исходный owner не
превращается в выдуманное успешное native поведение.

Missing owner/actor/command storage даёт аналитический отказ перед следующим
неразрешённым доступом. Уже записанный command prefix сохраняется, а старый
own target остаётся до завершения всех шести stores. Это граница адаптера,
а не native fault recovery или rollback. Данные стабильны внутри вызова;
явные замены owner/actor/command между вызовами — новые входы. Aliasing,
конкурентные изменения и owning lifetime внешних записей не квалифицированы.

[Проверки](../../../Winx/Tests/wxDarcyAttackAIActionTests.cpp) отдельно покрывают
целый Exit, повторные вызовы, полные command/raw own bytes, свежие storage
наблюдения, сохранённый prefix при отказе и borrowed lifetime. Сравнение Entry
с общим Registry сохраняется отдельно; цельный native Entry→Exit сценарий
пока не следует из двух независимых method profiles.
