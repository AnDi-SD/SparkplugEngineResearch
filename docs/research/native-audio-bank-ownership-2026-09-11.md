# AudioBank: удаление, владение и восстановление default bank

36 квалифицированных main transactions:18 PC и18 PS2. PC полностью
исполняет операции удаления иконструкторы банков. PS2 даёт6 whole returns,
10 остановок перед освобождением банка и2 перед default-bank factory.
Это разный объём подтверждения,не полная platform parity или проверка звука.

## Менеджер и первый подходящий банк

PC inherited method48F9B0 иPS21232E0 ищут первое совпадение bank word10
с переданным identifier. PC pointer-vector:begin84/end88/capacity8C;
PS2:capacity7C/count80/data84. Найденный объект передаётся настоящему
deleting destructor сargument1. После его возврата последняя запись
замещает найденную,число элементов уменьшается. Порядок остальных записей
не сортируется;duplicate identifiers не означают удаление всех совпадений.

Если identifier==1,метод затем создаёт новый concrete bank,пишет емуword10=1
и добавляет вмассив. Это происходит также при первоначально пустом массиве.
На квалифицированных полных ветвях возвращаетсяtrue;успех поиска не является
условием return. PC пустой список имеет выделенную fixture capacity4,
поэтому дальнейшая реаллокация vector за пределами capacity не заявлена.

Матрица:удаление первого/среднего/последнего из[2,3,4],отсутствующий9,
пустой список с9/1,[1,3] сidentifier1 и[2,3,3] сidentifier3;две заливки.
PC доказал конечный swap/count и4 полных создания default bank внутри
операций. PS2 при найденном банке останавливается на10D810 **после**
реального base teardown,но до platform deallocator:массив менеджера ещё
не изменён. Дальнейшие swap/count подтверждены кодом,не исполнены здесь.
PS2 empty identifier1 дошёл до1EAAC0;factory result не подставлен.

## Разрушение пустого банка

PC concrete factory4C7450 создаёт настоящий40-byte spDXAudioBank с
пустым,но владеющим выделенным storage вектором. Все36 подготовительных
factory calls выполнялись на original PC коде;они отделены от36 main
transactions. Existing fixture подменяет только allocation/free иSEH storage,
не банковую логику. Original deleting dtor4C7510→4C7440→49E730
освобождает storage,обнуляет1C/20/24 ивызывает4102B0. Последний пишет
base table6DAEB8. При deleting flag1 освобождается такжесам bank;
при direct flag0 bank остаётся,storage освобождается.

PS2 concrete1EA940 снулевым SDK handle24 вызывает base121500.
Пустой vector18 имеетcount1C=0 иdata20=0,owner extension4=0. Выполняются
настоящие111680/111660 iterators,101250 vector destructor и102B50 base
destructor,конечная таблица48C5E0. Direct deleting flag0 полностью
возвращается без allocator callback;flag1 приводит к явной границе10D810.
Global delete wrapper читает callback поGP−7F34:его результат не заменён.

Начальное владение storage различно иуказано явно:PC actual factory с
выделенным пустым capacity,PS2 literal zero-capacity fixture. Populated
bank entries,ненулевой SDK handle,owner extension иnormal PS2 factory
здесь не подтверждены. Base/derived identification дополнен оригинальными
записями таблиц,не только одинаковыми getter или class ID.

## Проверка и исправленное ожидание

Первый PC пилот завершил original teardown,но failed assertion ожидал
неверный base-table address6DA6E4. Original4102B9 явно пишет6DAEB8;
исправлен только oracle,ранний source/result сохранены. Всего37 main
attempts,36 successes:pilot1 /0,218278s,pilot2 4 /0,477378s,batch32
/3,161315s. Ни один успешный main case не повторялся.

24 actual concrete destructor entries,22 PC frees через проверенный
allocator fixture,280 PS2 SQ/LQ operations. Guard8KiB покрывает manager
fixture иarray,атакже literal PS2 banks;PC factory allocations находятся
отдельно и проверяются через allocation ledger иконечные поля teardown.
Whole PS2 calls восстанавливают stack/callee-saved/upper64,code неизменен.
2 PS2 factory boundary observations не считаются completed factory calls.

4 assessments +5:PC Bank20→25/Manager25→30,PS2 Bank15→20/Manager20→25.
Base AudioBank/AudioManager повторного балла не получают. Новых C++ и
полностью закрытых классов нет. Продолжение PS2 allocator/SDK,default
factory иплотные массивы entries остаются открытыми.

[Контракт и версии](../../research/audio-bank-ownership-contracts-2026-09-11.json),
[platform manifest](../../research/native-platform-audio-bank-ownership-2026-09-11.json).
Локальные runs: `local-data/results/native-cycle-20260911-0730/audio-bank-ownership/`.
