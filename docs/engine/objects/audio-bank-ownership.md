# AudioBank: удаление, владение и восстановление default bank

## Менеджер и первый подходящий банк

PC inherited method48F9B0 иPS21232E0 ищут первое совпадение bank word10
с переданным identifier. PC pointer-vector:begin84/end88/capacity8C;
PS2:capacity7C/count80/data84. Найденный объект передаётся настоящему
deleting destructor сargument1. После его возврата последняя запись
замещает найденную,число элементов уменьшается. Порядок остальных записей
не сортируется;duplicate identifiers не означают удаление всех совпадений.

Матрица:удаление первого/среднего/последнего из[2,3,4],отсутствующий9,
пустой список с9/1,[1,3] сidentifier1 и[2,3,3] сidentifier3;две заливки.
PC доказал конечный swap/count и4 полных создания default bank внутри
операций. PS2 при найденном банке останавливается на10D810 **после**
реального base teardown,но до platform deallocator:массив менеджера ещё
не изменён. Дальнейшие swap/count подтверждены кодом,не исполнены здесь.
PS2 empty identifier1 дошёл до1EAAC0;factory result не подставлен.

## Разрушение пустого банка

PS2 concrete1EA940 снулевым SDK handle24 вызывает base121500.
Пустой vector18 имеетcount1C=0 иdata20=0,owner extension4=0. Выполняются
настоящие111680/111660 iterators,101250 vector destructor и102B50 base
destructor,конечная таблица48C5E0. Direct deleting flag0 полностью
возвращается без allocator callback;flag1 приводит к явной границе10D810.
Global delete wrapper читает callback поGP−7F34:его результат не заменён.

4 assessments +5:PC Bank20→25/Manager25→30,PS2 Bank15→20/Manager20→25.
Base AudioBank/AudioManager повторного балла не получают. Новых C++ и
полностью закрытых классов нет. Продолжение PS2 allocator/SDK,default
factory иплотные массивы entries остаются открытыми.
