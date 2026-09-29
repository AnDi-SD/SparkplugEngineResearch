# spEntity

`spEntity` имеет Class ID `22875AA1` и физически наследует `spNamedObject`
на PC и PS2. Объект занимает `28` hex байт. Переносимая реализация
находится в [spEntity.h](../../../Sparkplug/Code/Sparkplug/spEntity.h) и
[spEntity.cpp](../../../Sparkplug/Code/Sparkplug/spEntity.cpp); внешние
операции менеджера и памяти передаются через
[spEntityHost](../../../Sparkplug/Analysis/Host/spEntityHost.h).

| Контракт | PC | PS2 |
| --- | ---: | ---: |
| Factory | `419D40` | `14E560` |
| Vtable | `6DBCCC` | `48DF60` |
| RTTI getter | `419BF0` | `14E140` |
| Clone | `419DA0` | собственный factory/clone contract |
| Назначение ссылки, slot 9 | `419D00` | `14E150` |

[PC ABI](../../../Sparkplug/Analysis/PC/spEntityAbi.h) и
[PS2 ABI](../../../Sparkplug/Analysis/PS2/spEntityAbi.h) фиксируют поля:
`+14/+18/+1C/+24=0`, `+20=3` после factory. PS2 factory содержит inline
construction с этими значениями и добавляет объект в engine manager;
отдельный constructor `14E390` не вызывается этим путём. PC original factory,
RTTI, default clone и оба удаления прошли исполнение.

Назначение ссылки `+18` при равных указателях ничего не делает, включая
null/null. Иначе прежнему объекту уменьшает 16-битный счётчик `+8`; при
получении нуля вызывает deleting destructor, пока receiver ещё содержит
старый указатель. Новому ненулевому объекту увеличивает такой же счётчик,
затем записывает указатель в receiver. Арифметика оборачивается по модулю
`2^16`: `0→FFFF` при уменьшении и `FFFF→0` при увеличении. Эти ветви
проверены в оригинале на PC и соответствующих PS2 prefixes.

Составной native destructor помимо ссылки `+18` освобождает объекты `+14/+1C`
и удаляет entity из manager. Его полный порядок при ненулевых зависимостях
ещё не восстановлен; переносимый destructor вызывает явный host boundary,
не выдавая пустую очистку за поведение игры. Тест `SparkEntityTests`
проверяет factory/default clone, короткую ветвь одинаковых указателей,
порядок retain/release/delete и граничные значения счётчика.
